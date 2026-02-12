use std::env;
use std::fs;
use std::io::{self, Read, IsTerminal};
use std::process::{Command, Stdio};
use std::collections::{HashMap, HashSet};
use std::sync::Once;
use std::path::PathBuf;

use pest::Parser;
use pest::iterators::Pairs;
use pest_derive::Parser;

mod scope;
mod value;
mod ast;

#[derive(Parser)]
#[grammar = "grammar.pest"]
struct MiniParser;

#[derive(Debug, Clone)]
struct MacroDef {
  params: Vec<String>,
  variadic: bool,
  body: String,
}

fn main() {
  let target_triple = detect_target_triple();
  let options = parse_args();

  // Step 1: Read and preprocess
  let (input, source_name) = read_input(options.input_path.clone());
  let base_dir = source_name.as_deref().and_then(|p| PathBuf::from(p).parent().map(|d| d.to_path_buf()));
  let (included, extra_objects) = match expand_includes(&input, base_dir) {
    Ok(result) => result,
    Err(err) => {
      eprintln!("Error: {err}");
      std::process::exit(1);
    }
  };
  let preprocessed = preprocess_macros(&included);

  // Step 2: Parse
  match MiniParser::parse(Rule::program, &preprocessed) {
    Ok(pairs) => {
      if !has_user_main(pairs.clone()) {
        eprintln!("Error: no main function defined");
        std::process::exit(1);
      }

      // Step 3: Generate unoptimized LLVM IR
      let unopt_ir = emit_llvm_ir(pairs, source_name.as_deref().unwrap_or("<stdin>"), &target_triple);

      // Step 4: Optimize based on chosen optimization level
      let opt_level = options.opt_level.as_deref().unwrap_or("-O2");
      let ir = match run_opt(&unopt_ir, opt_level) {
        Ok(optimized) => optimized,
        Err(err) => {
          // If optimization fails, emit a warning but continue with unoptimized IR
          eprintln!("warning: optimization failed: {err}");
          unopt_ir.clone()
        }
      };

      let display_unopt_ir = strip_externs_for_display(&unopt_ir);
      let display_opt_ir = strip_externs_for_display(&ir);

      // Step 5: Emit or compile based on output format
      match options.output_format.as_deref() {
        Some("-LL") | Some("-MIR") => {
          // Output unoptimized IR
          match resolve_text_output_target(options.output_path.as_deref(), options.input_path.as_deref(), options.output_format.as_deref()) {
            Ok(OutputTarget::Stdout) => println!("{}", display_unopt_ir),
            Ok(OutputTarget::File(path)) => {
              if let Err(err) = fs::write(&path, display_unopt_ir) {
                eprintln!("Error: failed to write output file: {err}");
                std::process::exit(1);
              }
            }
            Err(err) => {
              eprintln!("Error: {err}");
              std::process::exit(1);
            }
          }
        }
        Some("-LLM") => {
          // Output optimized LLVM IR
          match resolve_text_output_target(options.output_path.as_deref(), options.input_path.as_deref(), options.output_format.as_deref()) {
            Ok(OutputTarget::Stdout) => println!("{}", display_opt_ir),
            Ok(OutputTarget::File(path)) => {
              if let Err(err) = fs::write(&path, display_opt_ir) {
                eprintln!("Error: failed to write output file: {err}");
                std::process::exit(1);
              }
            }
            Err(err) => {
              eprintln!("Error: {err}");
              std::process::exit(1);
            }
          }
        }
        Some("-S") => {
          // Output assembly (use optimized IR if available)
          match resolve_text_output_target(options.output_path.as_deref(), options.input_path.as_deref(), options.output_format.as_deref()) {
            Ok(OutputTarget::Stdout) => {
              if let Err(err) = run_llc(&ir, "-", "asm") {
                eprintln!("llc failed: {err}");
                std::process::exit(1);
              }
            }
            Ok(OutputTarget::File(path)) => {
              if let Err(err) = run_llc(&ir, &path, "asm") {
                eprintln!("llc failed: {err}");
                std::process::exit(1);
              }
            }
            Err(err) => {
              eprintln!("Error: {err}");
              std::process::exit(1);
            }
          }
        }
        Some("-OBJ") => {
          // Output object file only (no linking)
          let output_path = resolve_binary_output_path(options.output_path.as_deref(), options.input_path.as_deref(), options.output_format.as_deref());
          if let Some(path) = output_path {
            if let Err(err) = compile_to_object(&ir, &path) {
              eprintln!("Error: failed to compile to object: {err}");
              std::process::exit(1);
            }
          } else {
            eprintln!("Error: no output file specified for object output");
            std::process::exit(1);
          }
        }
        _ => {
          // Default: compile and link to binary
          let output_path = resolve_binary_output_path(options.output_path.as_deref(), options.input_path.as_deref(), options.output_format.as_deref());
          if let Some(path) = output_path {
            if let Err(err) = compile_to_binary(&ir, &path, &target_triple, &extra_objects) {
              eprintln!("Error: failed to compile to binary: {err}");
              std::process::exit(1);
            }
          } else {
            eprintln!("Error: no output file specified for binary");
            std::process::exit(1);
          }
        }
      }
    }
    Err(err) => {
      eprintln!("Parse error:\n{}", err);
      std::process::exit(1);
    }
  }
}

fn strip_externs_for_display(ir: &str) -> String {
  ir
    .lines()
    .filter(|line| !line.trim_start().starts_with("declare "))
    .collect::<Vec<_>>()
    .join("\n")
}

enum OutputTarget {
  Stdout,
  File(String),
}

fn resolve_text_output_target(
  output_path: Option<&str>,
  input_path: Option<&str>,
  output_format: Option<&str>,
) -> Result<OutputTarget, String> {
  if let Some(path) = output_path {
    if path == "-" {
      return Ok(OutputTarget::Stdout);
    }
    return Ok(OutputTarget::File(path.to_string()));
  }

  if !io::stdout().is_terminal() {
    return Ok(OutputTarget::Stdout);
  }

  if let Some(path) = default_output_path(input_path, output_format) {
    return Ok(OutputTarget::File(path));
  }

  Err("no output file specified".to_string())
}

fn resolve_binary_output_path(
  output_path: Option<&str>,
  input_path: Option<&str>,
  output_format: Option<&str>,
) -> Option<String> {
  if let Some(path) = output_path {
    return Some(path.to_string());
  }
  default_output_path(input_path, output_format)
}

fn default_output_path(input_path: Option<&str>, output_format: Option<&str>) -> Option<String> {
  let input_path = input_path?;
  let path = PathBuf::from(input_path);
  let stem = path.file_stem()?.to_string_lossy();
  let parent = path.parent();

  let file_name = match output_format {
    Some("-LL") => format!("{}.ll", stem),
    Some("-LLM") => format!("{}.opt.ll", stem),
    Some("-MIR") => format!("{}.mir", stem),
    Some("-S") => format!("{}.s", stem),
    Some("-OBJ") => format!("{}.o", stem),
    _ => {
      if cfg!(target_os = "windows") {
        format!("{}.exe", stem)
      } else {
        stem.to_string()
      }
    }
  };

  let out_path = match parent {
    Some(dir) => dir.join(file_name),
    None => PathBuf::from(file_name),
  };
  Some(out_path.to_string_lossy().to_string())
}

fn detect_target_triple() -> String {
  if cfg!(target_os = "windows") {
    if cfg!(target_arch = "x86_64") {
      "x86_64-pc-windows-msvc".to_string()
    } else if cfg!(target_arch = "aarch64") {
      "aarch64-pc-windows-msvc".to_string()
    } else {
      "i686-pc-windows-msvc".to_string()
    }
  } else if cfg!(target_os = "macos") {
    if cfg!(target_arch = "x86_64") {
      "x86_64-apple-macosx10.7.0".to_string()
    } else if cfg!(target_arch = "aarch64") {
      "aarch64-apple-darwin".to_string()
    } else {
      "x86_64-apple-macosx10.7.0".to_string()
    }
  } else if cfg!(target_os = "linux") {
    if cfg!(target_arch = "x86_64") {
      "x86_64-unknown-linux-gnu".to_string()
    } else if cfg!(target_arch = "aarch64") {
      "aarch64-unknown-linux-gnu".to_string()
    } else {
      "i686-unknown-linux-gnu".to_string()
    }
  } else {
    // Default fallback
    "x86_64-unknown-linux-gnu".to_string()
  }
}

#[derive(Debug, Clone, Default)]
struct CliOptions {
  input_path: Option<String>,
  output_path: Option<String>,
  output_format: Option<String>, // -S, -LL, -LLM, -MIR, -OBJ, or none for binary
  opt_level: Option<String>, // -O0, -O1, -O2, -O3
}

fn parse_args() -> CliOptions {
  let mut options = CliOptions::default();
  let mut args = env::args().skip(1);
  let mut expect_output = false;

  while let Some(arg) = args.next() {
    if expect_output {
      options.output_path = Some(arg);
      expect_output = false;
      continue;
    }

    match arg.as_str() {
      "-o" => {
        expect_output = true;
      }
      "-S" | "-LL" | "-LLM" | "-MIR" | "-OBJ" | "-BC" => {
        options.output_format = Some(arg);
      }
      "-O0" | "-O1" | "-O2" | "-O3" => {
        options.opt_level = Some(arg);
      }
      _ if options.input_path.is_none() && !arg.starts_with('-') => {
        options.input_path = Some(arg);
      }
      _ => {}
    }
  }

  options
}

fn read_input(path: Option<String>) -> (String, Option<String>) {
  if let Some(path) = path {
    let input = fs::read_to_string(&path).unwrap_or_else(|err| {
      eprintln!("Failed to read input file: {err}");
      std::process::exit(1);
    });
    (input, Some(path))
  } else {
    let mut buf = String::new();
    io::stdin().read_to_string(&mut buf).unwrap_or_else(|err| {
      eprintln!("Failed to read stdin: {err}");
      std::process::exit(1);
    });
    (buf, None)
  }
}

fn expand_includes(input: &str, base_dir: Option<PathBuf>) -> Result<(String, Vec<String>), String> {
  let mut visited: HashSet<PathBuf> = HashSet::new();
  let mut objects: Vec<String> = Vec::new();
  let expanded = expand_includes_inner(input, base_dir, &mut visited, &mut objects)?;
  Ok((expanded, objects))
}

fn expand_includes_inner(
  input: &str,
  base_dir: Option<PathBuf>,
  visited: &mut HashSet<PathBuf>,
  objects: &mut Vec<String>,
) -> Result<String, String> {
  let mut output = String::new();
  for line in input.lines() {
    let trimmed = line.trim_start();
    if trimmed.starts_with("include ") {
      let end = trimmed.rfind(';').ok_or_else(|| "include statement must end with ';'".to_string())?;
      let content = trimmed[..end].trim_start_matches("include").trim();
      let (path_text, is_string) = parse_include_path(content)?;
      if !is_string {
        return Err("include path must be a string literal".to_string());
      }

      let include_path = resolve_include_path(&path_text, base_dir.as_ref())?;
      let ext = include_path.extension().and_then(|e| e.to_str()).unwrap_or("");
      if ext.eq_ignore_ascii_case("mi") {
        if visited.contains(&include_path) {
          continue;
        }
        visited.insert(include_path.clone());
        let text = fs::read_to_string(&include_path)
          .map_err(|err| format!("failed to read include file {}: {err}", include_path.display()))?;
        let next_base = include_path.parent().map(|p| p.to_path_buf());
        let expanded = expand_includes_inner(&text, next_base, visited, objects)?;
        output.push_str(&expanded);
        output.push('\n');
        continue;
      }

      if ext.eq_ignore_ascii_case("o") || ext.eq_ignore_ascii_case("obj") {
        objects.push(include_path.to_string_lossy().to_string());
        continue;
      }

      return Err(format!("unsupported include file type: {}", include_path.display()));
    }

    output.push_str(line);
    output.push('\n');
  }

  Ok(output)
}

fn parse_include_path(text: &str) -> Result<(String, bool), String> {
  let trimmed = text.trim();
  if let Some(rest) = trimmed.strip_prefix('"') {
    if let Some(end) = rest.rfind('"') {
      return Ok((rest[..end].to_string(), true));
    }
  }
  Ok((trimmed.to_string(), false))
}

fn resolve_include_path(path_text: &str, base_dir: Option<&PathBuf>) -> Result<PathBuf, String> {
  let path = PathBuf::from(path_text);
  if path.is_absolute() {
    return Ok(path);
  }
  if let Some(dir) = base_dir {
    return Ok(dir.join(path));
  }
  Ok(path)
}

fn run_opt(ir: &str, level: &str) -> Result<String, String> {
  let mut child = Command::new("opt")
    .arg(level)
    .arg("-S")
    .stdin(Stdio::piped())
    .stdout(Stdio::piped())
    .stderr(Stdio::piped())
    .spawn()
    .map_err(|err| format!("failed to start opt: {err}"))?;

  if let Some(mut stdin) = child.stdin.take() {
    use std::io::Write;
    stdin
      .write_all(ir.as_bytes())
      .map_err(|err| format!("failed to write IR to opt: {err}"))?;
  }

  let output = child
    .wait_with_output()
    .map_err(|err| format!("failed to wait on opt: {err}"))?;

  if !output.status.success() {
    let stderr = String::from_utf8_lossy(&output.stderr);
    return Err(stderr.trim().to_string());
  }

  let stdout = String::from_utf8_lossy(&output.stdout).to_string();
  Ok(stdout)
}

fn run_llvm_as(ir: &str, output_path: &str) -> Result<(), String> {
  let mut child = Command::new("llvm-as")
    .arg("-o")
    .arg(output_path)
    .stdin(Stdio::piped())
    .stdout(Stdio::piped())
    .stderr(Stdio::piped())
    .spawn()
    .map_err(|err| format!("failed to start llvm-as: {err}"))?;

  if let Some(mut stdin) = child.stdin.take() {
    use std::io::Write;
    stdin
      .write_all(ir.as_bytes())
      .map_err(|err| format!("failed to write IR to llvm-as: {err}"))?;
  }

  let output = child
    .wait_with_output()
    .map_err(|err| format!("failed to wait on llvm-as: {err}"))?;

  if !output.status.success() {
    let stderr = String::from_utf8_lossy(&output.stderr);
    return Err(stderr.trim().to_string());
  }

  Ok(())
}

fn run_llc(ir: &str, output_path: &str, filetype: &str) -> Result<(), String> {
  let mut cmd = Command::new("llc");
  cmd.arg(format!("-filetype={}", filetype))
    .arg("-o")
    .arg(output_path)
    .stdin(Stdio::piped())
    .stdout(Stdio::piped())
    .stderr(Stdio::piped());

  let mut child = cmd
    .spawn()
    .map_err(|err| format!("failed to start llc: {err}"))?;

  if let Some(mut stdin) = child.stdin.take() {
    use std::io::Write;
    stdin
      .write_all(ir.as_bytes())
      .map_err(|err| format!("failed to write IR to llc: {err}"))?;
  }

  let output = child
    .wait_with_output()
    .map_err(|err| format!("failed to wait on llc: {err}"))?;

  if !output.status.success() {
    let stderr = String::from_utf8_lossy(&output.stderr);
    return Err(stderr.trim().to_string());
  }

  // If writing to stdout, print the assembly
  if output_path == "-" {
    let stdout = String::from_utf8_lossy(&output.stdout);
    println!("{}", stdout);
  }

  Ok(())
}

fn compile_to_object(ir: &str, output_path: &str) -> Result<(), String> {
  run_llc(ir, output_path, "obj")
}

fn compile_to_binary(ir: &str, output_path: &str, _target_triple: &str, extra_objects: &[String]) -> Result<(), String> {
  let obj_path = PathBuf::from(output_path)
    .with_extension("o")
    .to_string_lossy()
    .to_string();

  compile_to_object(ir, &obj_path)?;
  link_object(&obj_path, output_path, extra_objects)
}

fn link_object(obj_path: &str, output_path: &str, extra_objects: &[String]) -> Result<(), String> {
  let mut candidates: Vec<&str> = vec!["cc", "clang", "gcc"];
  if cfg!(target_os = "windows") {
    candidates = vec!["clang", "gcc", "cc"];
  }

  let mut last_err: Option<String> = None;
  for tool in candidates {
    let mut cmd = Command::new(tool);
    cmd.arg(obj_path)
      .arg("-o")
      .arg(output_path);
    for obj in extra_objects {
      cmd.arg(obj);
    }
    if cfg!(target_os = "linux") {
      cmd.arg("-no-pie");
    }
    let output = cmd
      .stdout(Stdio::piped())
      .stderr(Stdio::piped())
      .output();

    match output {
      Ok(out) if out.status.success() => return Ok(()),
      Ok(out) => {
        let stderr = String::from_utf8_lossy(&out.stderr);
        last_err = Some(format!("{tool}: {}", stderr.trim()));
      }
      Err(err) => {
        last_err = Some(format!("{tool}: failed to start linker: {err}"));
      }
    }
  }

  Err(last_err.unwrap_or_else(|| "no suitable system linker found".to_string()))
}

#[derive(Debug, Clone)]
enum TypeChoice {
  Single(String),
  Variant(Vec<String>, bool),
}

static VARIANT_WARN_ONCE: Once = Once::new();

#[derive(Debug, Clone)]
struct FuncTemplate {
  name: String,
  ret: TypeChoice,
  params: Vec<(TypeChoice, String)>,
  has_variant: bool,
  block_text: String,
}

fn emit_llvm_ir(pairs: Pairs<Rule>, source_name: &str, target_triple: &str) -> String {
  let mut templates: Vec<FuncTemplate> = Vec::new();
  let mut module = ModuleContext::new();
  let mut defined_names: HashSet<String> = HashSet::new();

  for pair in pairs.clone() {
    collect_functions(pair.clone(), &mut templates);
    collect_classes(pair, &mut module);
  }

  if let Err(err) = collect_global_vars(pairs.clone(), &mut module) {
    eprintln!("Error: {err}");
    std::process::exit(1);
  }

  let mut functions: Vec<String> = Vec::new();

  for template in &templates {
    let instances = instantiate_variants(template);
    for inst in instances {
      defined_names.insert(inst.name);
    }
  }

  let class_defs: Vec<ClassDef> = module.classes.values().cloned().collect();
  for class_def in &class_defs {
    for method in &class_def.methods {
      defined_names.insert(mangle_method_name(&class_def.name, &method.name));
    }
  }

  module.defined_funcs = defined_names.clone();

  let mut out = String::new();
  out.push_str("; ModuleID = 'minis'\n");
  out.push_str(&format!("source_filename = \"{}\"\n", source_name));
  out.push_str(&format!("target triple = \"{}\"\n\n", target_triple));

  // Render struct type definitions
  for struct_def in module.render_structs() {
    out.push_str(&struct_def);
    out.push('\n');
  }
  if !module.classes.is_empty() {
    out.push('\n');
  }

  for template in templates {
    let instances = instantiate_variants(&template);
    for inst in instances {
      functions.push(emit_function_ir(&inst, &mut module));
    }
  }

  for class_def in &class_defs {
    for method in &class_def.methods {
      let mut params: Vec<(String, String)> = Vec::new();
      params.push((format!("%{}*", class_def.name), "__self".to_string()));
      params.extend(method.params.clone());

      let inst = FuncInstance {
        name: mangle_method_name(&class_def.name, &method.name),
        ret: method.ret_ty.clone(),
        params,
        block_text: method.block_text.clone(),
        class_name: Some(class_def.name.clone()),
      };
      functions.push(emit_function_ir(&inst, &mut module));
    }
  }


  if !module.externs.is_empty() {
    let mut to_remove: Vec<String> = Vec::new();
    for decl in &module.externs {
      if let Some(name) = extern_decl_name(decl) {
        if defined_names.contains(&name) {
          to_remove.push(decl.clone());
        }
      }
    }
    for decl in to_remove {
      module.externs.remove(&decl);
    }
  }

  let globals = module.render_globals();
  for global in &globals {
    out.push_str(global);
    out.push('\n');
  }
  if !globals.is_empty() {
    out.push('\n');
  }

  let externs = module.render_externs();
  for decl in &externs {
    out.push_str(decl);
    out.push('\n');
  }
  if !externs.is_empty() {
    out.push('\n');
  }

  for (idx, func) in functions.iter().enumerate() {
    out.push_str(func);
    if idx + 1 < functions.len() {
      out.push('\n');
    }
  }

  out
}


#[derive(Debug, Clone)]
struct FuncInstance {
  name: String,
  ret: String,
  params: Vec<(String, String)>,
  block_text: String,
  class_name: Option<String>,
}

#[derive(Debug, Clone)]
struct StringConst {
  name: String,
  value: String,
  len: usize,
}

#[derive(Debug, Clone)]
struct GlobalVar {
  name: String,
  ty: String,
  init: String,
}

#[derive(Debug, Clone)]
struct ClassField {
  ty: String,
  name: String,
}

#[derive(Debug, Clone)]
struct ClassMethod {
  ret_ty: String,
  name: String,
  params: Vec<(String, String)>,
  block_text: String,
}

#[derive(Debug, Clone)]
struct ClassDef {
  name: String,
  fields: Vec<ClassField>,
  methods: Vec<ClassMethod>,
}

#[derive(Debug, Clone)]
struct ModuleContext {
  strings: Vec<StringConst>,
  string_map: HashMap<String, String>,
  externs: HashSet<String>,
  classes: HashMap<String, ClassDef>,
  defined_funcs: HashSet<String>,
  global_vars: Vec<GlobalVar>,
  global_map: HashMap<String, String>,
}

impl ModuleContext {
  fn new() -> Self {
    Self {
      strings: Vec::new(),
      string_map: HashMap::new(),
      externs: HashSet::new(),
      classes: HashMap::new(),
      defined_funcs: HashSet::new(),
      global_vars: Vec::new(),
      global_map: HashMap::new(),
    }
  }

  fn render_globals(&self) -> Vec<String> {
    let mut out: Vec<String> = self.strings
      .iter()
      .map(|s| format!("@{} = private unnamed_addr constant [{} x i8] c\"{}\"", s.name, s.len, s.value))
      .collect();
    for g in &self.global_vars {
      out.push(format!("@{} = global {} {}", g.name, g.ty, g.init));
    }
    out
  }

  fn render_externs(&self) -> Vec<String> {
    let mut decls: Vec<String> = self.externs
      .iter()
      .filter(|decl| {
        if let Some(name) = extern_decl_name(decl) {
          !self.defined_funcs.contains(&name)
        } else {
          true
        }
      })
      .cloned()
      .collect();
    decls.sort();
    decls
  }

  fn render_structs(&self) -> Vec<String> {
    let mut structs: Vec<String> = Vec::new();
    for (class_name, class_def) in &self.classes {
      let field_types: Vec<String> = class_def
        .fields
        .iter()
        .map(|f| llvm_type(&f.ty))
        .collect();
      let fields_str = field_types.join(", ");
      structs.push(format!("%{} = type {{ {} }}", class_name, fields_str));
    }
    structs
  }

  fn string_ptr(&mut self, text: &str) -> String {
    if let Some(name) = self.string_map.get(text) {
      return gep_for_string(name, text.len() + 1);
    }

    let (escaped, len) = escape_llvm_string(text);
    let name = format!(".str{}", self.strings.len());
    self.strings.push(StringConst {
      name: name.clone(),
      value: escaped,
      len,
    });
    self.string_map.insert(text.to_string(), name.clone());
    gep_for_string(&name, len)
  }

  fn add_global(&mut self, name: String, ty: String, init: String) {
    if self.global_map.contains_key(&name) {
      return;
    }
    self.global_vars.push(GlobalVar { name: name.clone(), ty: ty.clone(), init });
    self.global_map.insert(name, ty);
  }

  fn get_global_type(&self, name: &str) -> Option<String> {
    self.global_map.get(name).cloned()
  }
}

struct FunctionContext {
  locals: HashMap<String, (String, String)>,
  temp_idx: usize,
  label_idx: usize,
  lines: Vec<String>,
  ret_ty: String,
  class_name: Option<String>,
  self_name: Option<String>,
}

impl FunctionContext {
  fn new(ret_ty: String, class_name: Option<String>, self_name: Option<String>) -> Self {
    Self {
      locals: HashMap::new(),
      temp_idx: 0,
      label_idx: 0,
      lines: Vec::new(),
      ret_ty,
      class_name,
      self_name,
    }
  }

  fn new_temp(&mut self) -> String {
    let name = format!("%t{}", self.temp_idx);
    self.temp_idx += 1;
    name
  }

  fn new_label(&mut self, prefix: &str) -> String {
    let name = format!("{}.{}", prefix, self.label_idx);
    self.label_idx += 1;
    name
  }

  fn emit(&mut self, line: String) {
    self.lines.push(line);
  }
}

fn emit_function_ir(inst: &FuncInstance, module: &mut ModuleContext) -> String {
  let ret_ty = llvm_type(&inst.ret);
  let params = inst
    .params
    .iter()
    .map(|(ty, name)| format!("{} %{}", llvm_type(ty), name))
    .collect::<Vec<_>>()
    .join(", ");

  let self_name = inst.class_name.as_ref().map(|_| "__self".to_string());
  let mut func = FunctionContext::new(ret_ty.clone(), inst.class_name.clone(), self_name);
  for (ty, name) in &inst.params {
    let llvm_ty = llvm_type(ty);
    let slot = func.new_temp();
    func.emit(format!("{} = alloca {}", slot, llvm_ty));
    func.emit(format!("store {} %{}, {}* {}", llvm_ty, name, llvm_ty, slot));
    func.locals.insert(name.clone(), (llvm_ty, slot));
  }

  let mut terminated = false;
  if let Ok(mut pairs) = MiniParser::parse(Rule::block, &inst.block_text) {
    if let Some(block) = pairs.next() {
      terminated = emit_block_ir(block, &ret_ty, &mut func, module);
    }
  }

  if !terminated {
    func.emit(default_return(&ret_ty));
  }

  let mut body = String::new();
  for line in func.lines {
    if line.ends_with(':') {
      body.push_str(&line);
    } else {
      body.push_str("  ");
      body.push_str(&line);
    }
    body.push('\n');
  }

  format!(
    "define {} @{}({}) {{\n{}\n}}\n",
    ret_ty,
    inst.name,
    params,
    body.trim_end()
  )
}

fn default_return(llvm_ty: &str) -> String {
  if llvm_ty == "void" {
    "ret void".to_string()
  } else if llvm_ty.ends_with('*') {
    format!("ret {} null", llvm_ty)
  } else if llvm_ty == "double" {
    format!("ret {} 0.0", llvm_ty)
  } else {
    format!("ret {} 0", llvm_ty)
  }
}

fn llvm_type(type_name: &str) -> String {
  if type_name.starts_with('%') || type_name.ends_with('*') {
    return type_name.to_string();
  }
  match type_name {
    "i8" | "u8" => "i8".to_string(),
    "i16" | "ui16" => "i16".to_string(),
    "i32" | "ui32" => "i32".to_string(),
    "i64" | "ui64" | "int" => "i64".to_string(),
    "float" => "double".to_string(),
    "bool" => "i1".to_string(),
    "tribool" => "i8".to_string(),
    "str" | "list" | "dict" => "i8*".to_string(),
    "void" => "void".to_string(),
    _ => "i8*".to_string(),
  }
}

fn collect_functions(pair: pest::iterators::Pair<Rule>, out: &mut Vec<FuncTemplate>) {
  if pair.as_rule() == Rule::func_decl {
    if let Some(func) = parse_func_decl(pair) {
      out.push(func);
    }
    return;
  }

  for inner in pair.into_inner() {
    collect_functions(inner, out);
  }
}

fn collect_classes(pair: pest::iterators::Pair<Rule>, module: &mut ModuleContext) {
  if pair.as_rule() == Rule::class_decl {
    if let Some(class_def) = parse_class_decl(pair) {
      module.classes.insert(class_def.name.clone(), class_def);
    }
    return;
  }

  for inner in pair.into_inner() {
    collect_classes(inner, module);
  }
}

fn has_user_main(pairs: Pairs<Rule>) -> bool {
  fn search(pair: pest::iterators::Pair<Rule>) -> bool {
    if pair.as_rule() == Rule::func_decl {
      if let Some(func) = parse_func_decl(pair) {
        return func.name == "main";
      }
      return false;
    }

    for inner in pair.into_inner() {
      if search(inner) {
        return true;
      }
    }
    false
  }

  for pair in pairs {
    if search(pair) {
      return true;
    }
  }
  false
}

fn collect_global_vars(pairs: Pairs<Rule>, module: &mut ModuleContext) -> Result<(), String> {
  for pair in pairs {
    if pair.as_rule() != Rule::program {
      continue;
    }
    for top in pair.into_inner() {
      let stmt = if top.as_rule() == Rule::top_statement {
        top.into_inner().next()
      } else if top.as_rule() == Rule::statement {
        Some(top)
      } else {
        None
      };

      let Some(stmt) = stmt else { continue; };
      let inner = if stmt.as_rule() == Rule::statement {
        stmt.clone().into_inner().next()
      } else {
        Some(stmt.clone())
      };
      let Some(inner) = inner else { continue; };

      if inner.as_rule() == Rule::var_decl {
        add_global_from_var_decl(inner, module)?;
      }
    }
  }
  Ok(())
}

fn add_global_from_var_decl(pair: pest::iterators::Pair<Rule>, module: &mut ModuleContext) -> Result<(), String> {
  let mut inner = pair.into_inner();
  let mut type_name: Option<String> = None;
  let mut entries: Vec<(String, Option<pest::iterators::Pair<Rule>>)> = Vec::new();

  while let Some(next) = inner.next() {
    match next.as_rule() {
      Rule::type_spec | Rule::type_ref | Rule::class_name => {
        type_name = parse_type_name(next);
      }
      Rule::identifier => {
        entries.push((next.as_str().to_string(), None));
      }
      Rule::expr => {
        if let Some(last) = entries.last_mut() {
          last.1 = Some(next);
        }
      }
      _ => {}
    }
  }

  let type_name = type_name.ok_or_else(|| "global declaration missing type".to_string())?;
  if matches!(type_name.as_str(), "str" | "list" | "dict") {
    return Err("global string/list/dict values are not supported".to_string());
  }

  let llvm_ty = llvm_type(&type_name);
  for (name, expr) in entries {
    let init = if let Some(expr) = expr {
      eval_global_const(expr, &type_name, &llvm_ty)?
    } else {
      default_global_init(&llvm_ty)
    };
    module.add_global(name, llvm_ty.clone(), init);
  }
  Ok(())
}

fn default_global_init(llvm_ty: &str) -> String {
  if llvm_ty == "double" {
    "0.0".to_string()
  } else if llvm_ty.ends_with('*') {
    "null".to_string()
  } else {
    "0".to_string()
  }
}

fn is_unsigned_type(type_name: &str) -> bool {
  matches!(type_name, "u8" | "u16" | "u32" | "u64" | "ui8" | "ui16" | "ui32" | "ui64")
}

fn eval_global_const(pair: pest::iterators::Pair<Rule>, type_name: &str, llvm_ty: &str) -> Result<String, String> {
  let lit = eval_const_literal(pair).ok_or_else(|| "global initializer must be a constant literal".to_string())?;

  match lit {
    ConstLiteral::Int { text, radix, negative } => format_int_literal(&text, radix, negative, type_name, llvm_ty),
    ConstLiteral::Bool(b) => {
      let text = if b { "1" } else { "0" };
      format_int_literal(text, 10, false, type_name, llvm_ty)
    }
    ConstLiteral::Tribool(v) => {
      let text = v.to_string();
      format_int_literal(&text, 10, false, type_name, llvm_ty)
    }
    ConstLiteral::Float(text) => {
      if llvm_ty == "double" {
        Ok(text)
      } else {
        Err("global float initializer requires float type".to_string())
      }
    }
  }
}

fn format_int_literal(text: &str, radix: u32, negative: bool, type_name: &str, llvm_ty: &str) -> Result<String, String> {
  let width = int_width(llvm_ty).ok_or_else(|| "global integer initializer requires integer type".to_string())?;

  if type_name == "bool" {
    if negative {
      return Err("bool global initializer cannot be negative".to_string());
    }
    let value = parse_modulo(text, radix, width)?;
    if value == 0 || value == 1 {
      return Ok(value.to_string());
    }
    return Err("bool global initializer must be 0 or 1".to_string());
  }

  if type_name == "tribool" {
    if negative {
      return Err("tribool global initializer cannot be negative".to_string());
    }
    let value = parse_modulo(text, radix, width)?;
    if value <= 2 {
      return Ok(value.to_string());
    }
    return Err("tribool global initializer must be 0, 1, or 2".to_string());
  }

  let mut value = parse_modulo(text, radix, width)?;
  if negative {
    let modulus = 1_u128 << width;
    value = (modulus - (value % modulus)) % modulus;
  }

  if is_unsigned_type(type_name) {
    return Ok(value.to_string());
  }

  // For signed types, emit the wrapped bit pattern as unsigned decimal.
  Ok(value.to_string())
}

fn parse_modulo(text: &str, radix: u32, width: u32) -> Result<u128, String> {
  let modulus: u128 = 1_u128 << width;
  let mut acc: u128 = 0;
  for ch in text.chars() {
    let digit = ch.to_digit(radix).ok_or_else(|| "invalid digit in integer literal".to_string())? as u128;
    acc = (acc * radix as u128 + digit) % modulus;
  }
  Ok(acc)
}

enum ConstLiteral {
  Int { text: String, radix: u32, negative: bool },
  Float(String),
  Bool(bool),
  Tribool(i8),
}

fn eval_const_literal(pair: pest::iterators::Pair<Rule>) -> Option<ConstLiteral> {
  match pair.as_rule() {
    Rule::number => {
      let text = pair.as_str();
      if let Some(stripped) = text.strip_prefix('-') {
        Some(ConstLiteral::Int { text: stripped.to_string(), radix: 10, negative: true })
      } else {
        Some(ConstLiteral::Int { text: text.to_string(), radix: 10, negative: false })
      }
    }
    Rule::hex_number => {
      let text = pair.as_str().trim_start_matches("0x");
      Some(ConstLiteral::Int { text: text.to_string(), radix: 16, negative: false })
    }
    Rule::binary_number => {
      let text = pair.as_str().trim_start_matches("0b");
      Some(ConstLiteral::Int { text: text.to_string(), radix: 2, negative: false })
    }
    Rule::float => Some(ConstLiteral::Float(pair.as_str().to_string())),
    Rule::bool => Some(ConstLiteral::Bool(pair.as_str() == "true")),
    Rule::tribool_val => {
      let v = match pair.as_str() {
        "true" => 0,
        "false" => 1,
        _ => 2,
      };
      Some(ConstLiteral::Tribool(v))
    }
    Rule::expr |
    Rule::logical_expr |
    Rule::logical_and_expr |
    Rule::comparison |
    Rule::arith_expr |
    Rule::term |
    Rule::primary |
    Rule::primary_base => {
      let mut inner = pair.into_inner();
      let first = inner.next()?;
      if inner.next().is_none() {
        eval_const_literal(first)
      } else {
        None
      }
    }
    _ => None,
  }
}

fn parse_class_decl(pair: pest::iterators::Pair<Rule>) -> Option<ClassDef> {
  let mut inner = pair.into_inner();
  let class_name = inner.next()?.as_str().to_string();

  let mut fields: Vec<ClassField> = Vec::new();
  let mut methods: Vec<ClassMethod> = Vec::new();

  // Iterate through access sections
  for access_section in inner {
    if access_section.as_rule() != Rule::access_section {
      continue;
    }

    let mut section_inner = access_section.into_inner();
    // Skip the access_modifier (first element)
    let _ = section_inner.next();

    // Process members
    for member_wrapper in section_inner {
      // class_member is a wrapper, so we need to descend into it
      if member_wrapper.as_rule() == Rule::class_member {
        for member in member_wrapper.into_inner() {
          match member.as_rule() {
            Rule::class_field => {
              if let Some(field) = parse_class_field(member) {
                fields.push(field);
              }
            }
            Rule::class_method => {
              if let Some(method) = parse_class_method(member) {
                methods.push(method);
              }
            }
            _ => {}
          }
        }
      } else {
        // Direct member (shouldn't happen but handle it)
        match member_wrapper.as_rule() {
          Rule::class_field => {
            if let Some(field) = parse_class_field(member_wrapper) {
              fields.push(field);
            }
          }
          Rule::class_method => {
            if let Some(method) = parse_class_method(member_wrapper) {
              methods.push(method);
            }
          }
          _ => {}
        }
      }
    }
  }

  Some(ClassDef {
    name: class_name,
    fields,
    methods,
  })
}

fn parse_class_field(pair: pest::iterators::Pair<Rule>) -> Option<ClassField> {
  let mut inner = pair.into_inner();
  let type_pair = inner.next()?;

  // Try to parse the type; fallback to the string representation
  let ty_str = type_pair.as_str();
  let ty = parse_type_name(type_pair).or_else(|| {
    let s = ty_str.trim();
    if !s.is_empty() {
      Some(s.to_string())
    } else {
      None
    }
  })?;

  let name = inner.next()?.as_str().to_string();
  Some(ClassField { ty, name })
}

fn parse_class_method(pair: pest::iterators::Pair<Rule>) -> Option<ClassMethod> {
  let mut inner = pair.into_inner();
  let ret_type_pair = inner.next()?;
  let ret_ty = parse_type_name(ret_type_pair)?;
  let name = inner.next()?.as_str().to_string();

  let mut params: Vec<(String, String)> = Vec::new();
  let mut block_text = String::new();

  for next in inner {
    match next.as_rule() {
      Rule::param_list => {
        let parsed = parse_param_list(next);
        params = parsed.into_iter().map(|(ty, name)| {
          let ty_str = match ty {
            TypeChoice::Single(s) => s,
            TypeChoice::Variant(_, _) => "i64".to_string(), // Fallback for variants
          };
          (ty_str, name)
        }).collect();
      }
      Rule::block => {
        block_text = next.as_span().as_str().to_string();
      }
      _ => {}
    }
  }

  Some(ClassMethod {
    ret_ty,
    name,
    params,
    block_text,
  })
}

fn parse_func_decl(pair: pest::iterators::Pair<Rule>) -> Option<FuncTemplate> {
  let mut inner = pair.into_inner();
  let ret_pair = inner.next()?;
  let ret = parse_type_choice(ret_pair)?;
  let name = inner.next()?.as_str().to_string();

  let mut params: Vec<(TypeChoice, String)> = Vec::new();
  let mut block_text: Option<String> = None;
  for next in inner {
    match next.as_rule() {
      Rule::param_list => {
        params = parse_param_list(next);
      }
      Rule::block => {
        block_text = Some(next.as_span().as_str().to_string());
        break;
      }
      _ => {}
    }
  }

  let has_variant = has_variant_type(&ret) || params.iter().any(|(t, _)| has_variant_type(t));

  Some(FuncTemplate {
    name,
    ret,
    params,
    has_variant,
    block_text: block_text.unwrap_or_else(|| "{}".to_string()),
  })
}

fn parse_param_list(pair: pest::iterators::Pair<Rule>) -> Vec<(TypeChoice, String)> {
  let mut params = Vec::new();
  for param in pair.into_inner() {
    if param.as_rule() != Rule::param {
      continue;
    }
    let mut inner = param.into_inner();
    let type_pair = inner.next();
    let name_pair = inner.next();
    if let (Some(t), Some(n)) = (type_pair, name_pair) {
      if let Some(choice) = parse_type_choice(t) {
        params.push((choice, n.as_str().to_string()));
      }
    }
  }
  params
}

fn parse_type_choice(pair: pest::iterators::Pair<Rule>) -> Option<TypeChoice> {
  match pair.as_rule() {
    Rule::type_ref => {
      let mut inner = pair.into_inner();
      parse_type_choice(inner.next()?)
    }
    Rule::type_spec => {
      let mut inner = pair.into_inner();
      while let Some(next) = inner.next() {
        if matches!(next.as_rule(), Rule::base_type | Rule::variant_type | Rule::experimental_variant_type) {
          return parse_type_choice(next);
        }
      }
      None
    }
    Rule::experimental_variant_type => {
      let mut inner = pair.into_inner();
      let variant = inner.next()?;
      let types = variant
        .into_inner()
        .filter(|p| p.as_rule() == Rule::base_type)
        .map(|p| p.as_str().to_string())
        .collect::<Vec<_>>();
      if types.is_empty() {
        None
      } else {
        Some(TypeChoice::Variant(types, true))
      }
    }
    Rule::variant_type => {
      let types = pair
        .into_inner()
        .filter(|p| p.as_rule() == Rule::base_type)
        .map(|p| p.as_str().to_string())
        .collect::<Vec<_>>();
      if types.is_empty() {
        None
      } else {
        VARIANT_WARN_ONCE.call_once(|| {
          eprintln!("warning: variant<> is experimental; use !variant<> to accept experimental behavior");
        });
        Some(TypeChoice::Variant(types, false))
      }
    }
    Rule::base_type => Some(TypeChoice::Single(pair.as_str().to_string())),
    Rule::class_name => Some(TypeChoice::Single(format!("%{}*", pair.as_str()))),
    _ => None,
  }
}

fn has_variant_type(choice: &TypeChoice) -> bool {
  matches!(choice, TypeChoice::Variant(_, _))
}

fn instantiate_variants(template: &FuncTemplate) -> Vec<FuncInstance> {
  let mut choices: Vec<TypeChoice> = Vec::new();
  choices.push(template.ret.clone());
  for (t, _) in &template.params {
    choices.push(t.clone());
  }

  let combos = expand_type_choices(&choices);
  let mut instances: Vec<FuncInstance> = Vec::new();

  for combo in combos {
    let ret = combo[0].clone();
    let mut params: Vec<(String, String)> = Vec::new();
    for (idx, (_, name)) in template.params.iter().enumerate() {
      params.push((combo[idx + 1].clone(), name.clone()));
    }

    let name = if template.has_variant {
      mangle_name(&template.name, &combo)
    } else {
      template.name.clone()
    };

    instances.push(FuncInstance {
      name,
      ret,
      params,
      block_text: template.block_text.clone(),
      class_name: None,
    });
  }

  instances
}

#[derive(Debug, Clone)]
struct ValueIR {
  ty: String,
  val: String,
}

fn int_width(ty: &str) -> Option<u32> {
  match ty {
    "i1" => Some(1),
    "i8" => Some(8),
    "i16" => Some(16),
    "i32" => Some(32),
    "i64" => Some(64),
    _ => None,
  }
}

fn coerce_value_to_type(value: ValueIR, target_ty: &str, func: &mut FunctionContext) -> Option<ValueIR> {
  if value.ty == target_ty {
    return Some(value);
  }

  if let (Some(from_w), Some(to_w)) = (int_width(&value.ty), int_width(target_ty)) {
    let tmp = func.new_temp();
    if to_w > from_w {
      func.emit(format!("{} = sext {} {} to {}", tmp, value.ty, value.val, target_ty));
    } else if to_w < from_w {
      func.emit(format!("{} = trunc {} {} to {}", tmp, value.ty, value.val, target_ty));
    } else {
      return Some(ValueIR { ty: target_ty.to_string(), val: value.val });
    }
    return Some(ValueIR { ty: target_ty.to_string(), val: tmp });
  }

  if target_ty == "double" {
    if int_width(&value.ty).is_some() {
      let tmp = func.new_temp();
      func.emit(format!("{} = sitofp {} {} to {}", tmp, value.ty, value.val, target_ty));
      return Some(ValueIR { ty: target_ty.to_string(), val: tmp });
    }
  }

  if int_width(target_ty).is_some() && value.ty == "double" {
    let tmp = func.new_temp();
    func.emit(format!("{} = fptosi {} {} to {}", tmp, value.ty, value.val, target_ty));
    return Some(ValueIR { ty: target_ty.to_string(), val: tmp });
  }

  if value.ty.ends_with('*') && target_ty.ends_with('*') {
    let tmp = func.new_temp();
    func.emit(format!("{} = bitcast {} {} to {}", tmp, value.ty, value.val, target_ty));
    return Some(ValueIR { ty: target_ty.to_string(), val: tmp });
  }

  Some(value)
}

fn emit_block_ir(pair: pest::iterators::Pair<Rule>, ret_ty: &str, func: &mut FunctionContext, module: &mut ModuleContext) -> bool {
  emit_block_ir_with_return(pair, ret_ty, func, module, true)
}

fn emit_block_ir_with_return(
  pair: pest::iterators::Pair<Rule>,
  ret_ty: &str,
  func: &mut FunctionContext,
  module: &mut ModuleContext,
  allow_return: bool,
) -> bool {
  for stmt in pair.into_inner() {
    if emit_statement_ir_with_return(stmt, ret_ty, func, module, allow_return) {
      return true;
    }
  }
  false
}

fn emit_statement_ir(pair: pest::iterators::Pair<Rule>, ret_ty: &str, func: &mut FunctionContext, module: &mut ModuleContext) -> bool {
  emit_statement_ir_with_return(pair, ret_ty, func, module, true)
}

fn emit_statement_ir_with_return(
  pair: pest::iterators::Pair<Rule>,
  ret_ty: &str,
  func: &mut FunctionContext,
  module: &mut ModuleContext,
  allow_return: bool,
) -> bool {
  let inner_pair = if pair.as_rule() == Rule::statement {
    let mut inner = pair.clone().into_inner();
    if let Some(p) = inner.next() {
      p
    } else {
      return false;
    }
  } else {
    pair.clone()
  };

  match inner_pair.as_rule() {
    Rule::return_stmt => {
      let mut inner = inner_pair.into_inner();
      if !allow_return {
        if let Some(expr) = inner.next() {
          let _ = emit_expr_value(expr, func, module);
        }
        return false;
      }
      if let Some(expr) = inner.next() {
        if let Some(value) = emit_expr_value(expr, func, module) {
          if let Some(coerced) = coerce_value_to_type(value, ret_ty, func) {
            func.emit(format!("ret {} {}", coerced.ty, coerced.val));
            return true;
          }
        }
      }
      func.emit(default_return(ret_ty));
      return true;
    }
    Rule::assignment => {
      let mut inner = inner_pair.into_inner();
      let name = inner.next().map(|p| p.as_str().to_string());
      let expr = inner.next();
      if let (Some(name), Some(expr)) = (name, expr) {
        if let Some(value) = emit_expr_value(expr, func, module) {
          if !func.locals.contains_key(&name) {
            if emit_self_field_store(&name, &value, func, module) {
              return false;
            }
            if let Some(global_ty) = module.get_global_type(&name) {
              let stored = coerce_value_to_type(value, &global_ty, func).unwrap_or_else(|| ValueIR { ty: global_ty.clone(), val: "0".to_string() });
              func.emit(format!("store {} {}, {}* @{}", stored.ty, stored.val, global_ty, name));
              return false;
            }
            let slot = func.new_temp();
            func.emit(format!("{} = alloca {}", slot, value.ty));
            func.locals.insert(name.clone(), (value.ty.clone(), slot));
          }
          let slot = func.locals.get(&name).unwrap().clone();
          let stored = coerce_value_to_type(value, &slot.0, func).unwrap_or_else(|| ValueIR { ty: slot.0.clone(), val: "0".to_string() });
          func.emit(format!("store {} {}, {}* {}", stored.ty, stored.val, slot.0, slot.1));
        }
      }
      false
    }
    Rule::var_decl => {
      let mut inner = inner_pair.into_inner();
      let mut type_name: Option<String> = None;
      let mut last_slot: Option<(String, String)> = None;
      while let Some(next) = inner.next() {
        match next.as_rule() {
          Rule::type_spec | Rule::type_ref | Rule::class_name => {
            type_name = parse_type_name(next);
          }
          Rule::identifier => {
            let name = next.as_str().to_string();
            let llvm_ty = llvm_type(type_name.as_deref().unwrap_or("int"));
            let slot = func.new_temp();
            func.emit(format!("{} = alloca {}", slot, llvm_ty));
            func.locals.insert(name.clone(), (llvm_ty, slot));
            last_slot = func.locals.get(&name).cloned();
          }
          Rule::expr => {
            if let Some(value) = emit_expr_value(next, func, module) {
              if let Some((ty, slot)) = last_slot.clone() {
                let stored = coerce_value_to_type(value, &ty, func).unwrap_or_else(|| ValueIR { ty: ty.clone(), val: "0".to_string() });
                func.emit(format!("store {} {}, {}* {}", stored.ty, stored.val, ty, slot));
              }
            }
          }
          _ => {}
        }
      }
      false
    }
    Rule::function_call => {
      emit_call(inner_pair, func, module);
      false
    }
    Rule::expr => {
      let _ = emit_expr_value(inner_pair, func, module);
      false
    }
    Rule::if_stmt => {
      emit_if(inner_pair, ret_ty, func, module)
    }
    Rule::block => {
      emit_block_ir_with_return(inner_pair, ret_ty, func, module, allow_return)
    }
    _ => false,
  }
}

fn emit_if(pair: pest::iterators::Pair<Rule>, ret_ty: &str, func: &mut FunctionContext, module: &mut ModuleContext) -> bool {
  let mut inner = pair.into_inner();
  let cond_pair = inner.next();
  let then_block = inner.next();
  if cond_pair.is_none() || then_block.is_none() {
    return false;
  }

  let cond_val = emit_expr_value(cond_pair.unwrap(), func, module);
  let cond_val = match cond_val {
    Some(v) => v,
    None => return false,
  };

  let then_label = func.new_label("if.then");
  let else_label = func.new_label("if.else");
  let end_label = func.new_label("if.end");

  func.emit(format!("br i1 {}, label %{}, label %{}", cond_val.val, then_label, else_label));
  func.emit(format!("{}:", then_label));
  let then_term = emit_block_ir(then_block.unwrap(), ret_ty, func, module);
  if !then_term {
    func.emit(format!("br label %{}", end_label));
  }

  func.emit(format!("{}:", else_label));
  let mut else_term = false;
  for next in inner {
    if next.as_rule() == Rule::block {
      else_term = emit_block_ir(next, ret_ty, func, module);
      break;
    }
  }
  if !else_term {
    func.emit(format!("br label %{}", end_label));
  }

  func.emit(format!("{}:", end_label));
  then_term && else_term
}

fn emit_print(args: Vec<ValueIR>, func: &mut FunctionContext, module: &mut ModuleContext) {
  if args.is_empty() {
    return;
  }
  for (idx, arg) in args.iter().enumerate() {
    emit_print_value(arg, func, module);
    if idx + 1 < args.len() {
      emit_printf_literal(" ", func, module);
    }
  }
}

fn emit_print_value(arg: &ValueIR, func: &mut FunctionContext, module: &mut ModuleContext) {
  match arg.ty.as_str() {
    "i1" => {
      let true_ptr = module.string_ptr("true");
      let false_ptr = module.string_ptr("false");
      let tmp = func.new_temp();
      func.emit(format!("{} = select i1 {}, i8* {}, i8* {}", tmp, arg.val, true_ptr, false_ptr));
      emit_printf_call("%s", vec![ValueIR { ty: "i8*".to_string(), val: tmp }], func, module);
    }
    "i8*" => {
      emit_printf_call("%s", vec![arg.clone()], func, module);
    }
    "double" => {
      emit_printf_call("%f", vec![arg.clone()], func, module);
    }
    "i64" => {
      emit_printf_call("%lld", vec![arg.clone()], func, module);
    }
    "i32" | "i16" | "i8" => {
      emit_printf_call("%d", vec![arg.clone()], func, module);
    }
    _ if arg.ty.ends_with('*') => {
      let tmp = func.new_temp();
      func.emit(format!("{} = bitcast {} {} to i8*", tmp, arg.ty, arg.val));
      emit_printf_call("%p", vec![ValueIR { ty: "i8*".to_string(), val: tmp }], func, module);
    }
    _ => {
      emit_printf_call("%d", vec![arg.clone()], func, module);
    }
  }
}

fn emit_printf_literal(text: &str, func: &mut FunctionContext, module: &mut ModuleContext) {
  let ptr = module.string_ptr(text);
  emit_printf_call("%s", vec![ValueIR { ty: "i8*".to_string(), val: ptr }], func, module);
}

fn emit_printf_call(fmt: &str, values: Vec<ValueIR>, func: &mut FunctionContext, module: &mut ModuleContext) {
  module.externs.insert("declare i32 @printf(i8*, ...)".to_string());
  let fmt_ptr = module.string_ptr(fmt);
  let mut args: Vec<String> = Vec::new();
  args.push(format!("i8* {}", fmt_ptr));
  for v in values {
    args.push(format!("{} {}", v.ty, v.val));
  }
  func.emit(format!("call i32 @printf({})", args.join(", ")));
}

fn emit_call(pair: pest::iterators::Pair<Rule>, func: &mut FunctionContext, module: &mut ModuleContext) -> Option<ValueIR> {
  let mut inner = pair.into_inner();
  let name = inner.next()?.as_str().to_string();
  let mut args: Vec<ValueIR> = Vec::new();
  if let Some(list) = inner.next() {
    for expr in list.into_inner() {
      if let Some(val) = emit_expr_value(expr, func, module) {
        args.push(val);
      }
    }
  }

  match name.as_str() {
    "print" => {
      emit_print(args, func, module);
      None
    }
    "exit" => {
      module.externs.insert("declare void @exit(i64)".to_string());
      let arg = args.get(0).map(|v| format!("{} {}", v.ty, v.val)).unwrap_or("i64 0".to_string());
      func.emit(format!("call void @exit({})", arg));
      None
    }
    _ => {
      if let Some(class_name) = func.class_name.clone() {
        if let Some(class_def) = module.classes.get(&class_name) {
          if let Some(method_def) = class_def.methods.iter().find(|m| m.name == name) {
            let self_name = func.self_name.clone()?;
            let (self_ty, self_slot) = func.locals.get(&self_name)?.clone();
            let self_val = func.new_temp();
            func.emit(format!("{} = load {}, {}* {}", self_val, self_ty, self_ty, self_slot));
            args.insert(0, ValueIR { ty: self_ty, val: self_val });

            let mangled = mangle_method_name(&class_name, &method_def.name);
            let ret_ty = llvm_type(&method_def.ret_ty);
            let args_str = args
              .iter()
              .map(|v| format!("{} {}", v.ty, v.val))
              .collect::<Vec<_>>()
              .join(", ");

            if ret_ty == "void" {
              func.emit(format!("call void @{}({})", mangled, args_str));
              return None;
            }

            let tmp = func.new_temp();
            func.emit(format!("{} = call {} @{}({})", tmp, ret_ty, mangled, args_str));
            return Some(ValueIR { ty: ret_ty, val: tmp });
          }
        }
      }

      if !module.defined_funcs.contains(&name) {
        module.externs.insert(format!("declare i64 @{}(...)", name));
      }
      let args_str = args
        .iter()
        .map(|v| format!("{} {}", v.ty, v.val))
        .collect::<Vec<_>>()
        .join(", ");
      let tmp = func.new_temp();
      func.emit(format!("{} = call i64 @{}({})", tmp, name, args_str));
      Some(ValueIR { ty: "i64".to_string(), val: tmp })
    }
  }
}

fn emit_class_new(pair: pest::iterators::Pair<Rule>, func: &mut FunctionContext, module: &mut ModuleContext) -> Option<ValueIR> {
  let mut inner = pair.into_inner();
  let class_name = inner.next()?.as_str().to_string();

  if !module.classes.contains_key(&class_name) {
    return None;
  }

  // Allocate space for the struct: %obj = alloca %ClassName
  let tmp = func.new_temp();
  func.emit(format!("{} = alloca %{}", tmp, class_name));

  // For now, just return the allocated pointer
  // TODO: Initialize default field values
  Some(ValueIR { ty: format!("%{}*", class_name), val: tmp })
}

fn emit_method_call(pair: pest::iterators::Pair<Rule>, obj: ValueIR, func: &mut FunctionContext, module: &mut ModuleContext) -> Option<ValueIR> {
  let mut inner = pair.into_inner();
  let method_name = inner.next()?.as_str().to_string();

  let mut args: Vec<ValueIR> = vec![obj.clone()];
  if let Some(expr_list) = inner.next() {
    for expr in expr_list.into_inner() {
      if let Some(val) = emit_expr_value(expr, func, module) {
        args.push(val);
      }
    }
  }

  let class_name = if obj.ty.starts_with('%') && obj.ty.ends_with('*') {
    obj.ty[1..obj.ty.len() - 1].to_string()
  } else {
    return None;
  };

  if let Some(class_def) = module.classes.get(&class_name) {
    if let Some(method_def) = class_def.methods.iter().find(|m| m.name == method_name) {
      let mangled = mangle_method_name(&class_name, &method_def.name);
      let ret_ty = llvm_type(&method_def.ret_ty);
      let args_str = args
        .iter()
        .map(|v| format!("{} {}", v.ty, v.val))
        .collect::<Vec<_>>()
        .join(", ");

      if ret_ty == "void" {
        func.emit(format!("call void @{}({})", mangled, args_str));
        return None;
      }

      let tmp = func.new_temp();
      func.emit(format!("{} = call {} @{}({})", tmp, ret_ty, mangled, args_str));
      return Some(ValueIR { ty: ret_ty, val: tmp });
    }
  }

  let args_str = args
    .iter()
    .map(|v| format!("{} {}", v.ty, v.val))
    .collect::<Vec<_>>()
    .join(", ");
  let tmp = func.new_temp();
  module.externs.insert(format!("declare i64 @{}(...)", method_name));
  func.emit(format!("{} = call i64 @{}({})", tmp, method_name, args_str));
  Some(ValueIR { ty: "i64".to_string(), val: tmp })
}

fn emit_field_ptr(class_name: &str, field_name: &str, obj: &ValueIR, func: &mut FunctionContext, module: &ModuleContext) -> Option<(String, String)> {
  let class_def = module.classes.get(class_name)?;
  let field_idx = class_def.fields.iter().position(|f| f.name == field_name)?;
  let field_type = llvm_type(&class_def.fields[field_idx].ty);

  let ptr = func.new_temp();
  func.emit(format!("{} = getelementptr %{}, %{}* {}, i32 0, i32 {}",
    ptr, class_name, class_name, obj.val, field_idx));
  Some((field_type, ptr))
}

fn emit_self_field_load(field_name: &str, func: &mut FunctionContext, module: &mut ModuleContext) -> Option<ValueIR> {
  let class_name = func.class_name.as_ref()?.clone();
  let self_name = func.self_name.as_ref()?.clone();
  let (self_ty, self_slot) = func.locals.get(&self_name)?.clone();

  let self_val = func.new_temp();
  func.emit(format!("{} = load {}, {}* {}", self_val, self_ty, self_ty, self_slot));
  let obj = ValueIR { ty: self_ty, val: self_val };
  let (field_type, ptr) = emit_field_ptr(&class_name, field_name, &obj, func, module)?;

  let val = func.new_temp();
  func.emit(format!("{} = load {}, {}* {}", val, field_type, field_type, ptr));
  Some(ValueIR { ty: field_type, val })
}

fn emit_self_field_store(field_name: &str, value: &ValueIR, func: &mut FunctionContext, module: &mut ModuleContext) -> bool {
  let class_name = match func.class_name.as_ref() {
    Some(name) => name.clone(),
    None => return false,
  };
  let self_name = match func.self_name.as_ref() {
    Some(name) => name.clone(),
    None => return false,
  };
  let (self_ty, self_slot) = match func.locals.get(&self_name) {
    Some(info) => info.clone(),
    None => return false,
  };

  let self_val = func.new_temp();
  func.emit(format!("{} = load {}, {}* {}", self_val, self_ty, self_ty, self_slot));
  let obj = ValueIR { ty: self_ty, val: self_val };
  let (field_type, ptr) = match emit_field_ptr(&class_name, field_name, &obj, func, module) {
    Some(info) => info,
    None => return false,
  };

  func.emit(format!("store {} {}, {}* {}", field_type, value.val, field_type, ptr));
  true
}

fn emit_member_access(pair: pest::iterators::Pair<Rule>, obj: ValueIR, func: &mut FunctionContext, module: &mut ModuleContext) -> Option<ValueIR> {
  let mut inner = pair.into_inner();
  let field_name = inner.next()?.as_str().to_string();

  // Extract class name from object type (e.g., "%player*" -> "player")
  let class_name = if obj.ty.starts_with('%') && obj.ty.ends_with('*') {
    obj.ty[1..obj.ty.len()-1].to_string()
  } else {
    return None;
  };

  let (field_type, ptr) = emit_field_ptr(&class_name, &field_name, &obj, func, module)?;

  let val = func.new_temp();
  func.emit(format!("{} = load {}, {}* {}", val, field_type, field_type, ptr));

  Some(ValueIR { ty: field_type, val })
}

fn emit_expr_value(pair: pest::iterators::Pair<Rule>, func: &mut FunctionContext, module: &mut ModuleContext) -> Option<ValueIR> {
  match pair.as_rule() {
    Rule::expr |
    Rule::logical_expr |
    Rule::logical_and_expr |
    Rule::comparison => emit_comparison(pair, func, module),
    Rule::arith_expr => emit_arith_expr(pair, func, module),
    Rule::term => emit_term_expr(pair, func, module),
    Rule::primary => {
      let mut inner = pair.clone().into_inner();
      let base = inner.next()?;
      let mut result = emit_expr_value(base, func, module)?;

      // Process postfix operations (method calls, member access, indexing)
      for postfix in inner {
        match postfix.as_rule() {
          Rule::method_tail => {
            result = emit_method_call(postfix, result, func, module)?;
          }
          Rule::member_tail => {
            result = emit_member_access(postfix, result, func, module)?;
          }
          _ => {}
        }
      }
      Some(result)
    }
    Rule::primary_base => {
      let mut inner = pair.clone().into_inner();
      let first = inner.next()?;
      if inner.next().is_none() {
        emit_expr_value(first, func, module)
      } else {
        emit_arith_expr(pair, func, module)
      }
    }
    Rule::number => Some(ValueIR { ty: "i64".to_string(), val: pair.as_str().to_string() }),
    Rule::float => Some(ValueIR { ty: "double".to_string(), val: pair.as_str().to_string() }),
    Rule::hex_number => {
      let v = i64::from_str_radix(pair.as_str().trim_start_matches("0x"), 16).ok()?;
      Some(ValueIR { ty: "i64".to_string(), val: v.to_string() })
    }
    Rule::binary_number => {
      let v = i64::from_str_radix(pair.as_str().trim_start_matches("0b"), 2).ok()?;
      Some(ValueIR { ty: "i64".to_string(), val: v.to_string() })
    }
    Rule::string => {
      let stripped = pair.as_str().trim_matches('"');
      let text = stripped.replace("\\n", "\n").replace("\\t", "\t").replace("\\r", "\r");
      let ptr = module.string_ptr(&text);
      Some(ValueIR { ty: "i8*".to_string(), val: ptr })
    }
    Rule::bool => {
      let val = if pair.as_str() == "true" { "1" } else { "0" };
      Some(ValueIR { ty: "i1".to_string(), val: val.to_string() })
    }
    Rule::tribool_val => {
      let val = match pair.as_str() {
        "true" => "0",
        "false" => "1",
        _ => "2", // "unknown"
      };
      Some(ValueIR { ty: "i8".to_string(), val: val.to_string() })
    }
    Rule::identifier => {
      let local_info = func.locals.get(pair.as_str()).cloned();
      if let Some((ty, slot)) = local_info {
        let tmp = func.new_temp();
        func.emit(format!("{} = load {}, {}* {}", tmp, ty, ty, slot));
        Some(ValueIR { ty, val: tmp })
      } else {
        if let Some(val) = emit_self_field_load(pair.as_str(), func, module) {
          Some(val)
        } else if let Some(global_ty) = module.get_global_type(pair.as_str()) {
          let tmp = func.new_temp();
          func.emit(format!("{} = load {}, {}* @{}", tmp, global_ty, global_ty, pair.as_str()));
          Some(ValueIR { ty: global_ty, val: tmp })
        } else {
          Some(ValueIR { ty: "i64".to_string(), val: "0".to_string() })
        }
      }
    }
    Rule::function_call => emit_call(pair, func, module),
    Rule::class_instantiation => emit_class_new(pair, func, module),
    Rule::circuit_var => emit_circuit_var(pair, func, module),
    _ => None,
  }
}

fn emit_arith_expr(pair: pest::iterators::Pair<Rule>, func: &mut FunctionContext, module: &mut ModuleContext) -> Option<ValueIR> {
  let mut inner = pair.into_inner();
  let mut left = emit_expr_value(inner.next()?, func, module)?;
  while let Some(op) = inner.next() {
    let right = emit_expr_value(inner.next()?, func, module)?;
    let op_name = match op.as_rule() {
      Rule::add => "add",
      Rule::subtract => "sub",
      _ => return None,
    };
    let tmp = func.new_temp();
    func.emit(format!("{} = {} {} {}, {}", tmp, op_name, left.ty, left.val, right.val));
    left = ValueIR { ty: left.ty, val: tmp };
  }
  Some(left)
}

fn emit_term_expr(pair: pest::iterators::Pair<Rule>, func: &mut FunctionContext, module: &mut ModuleContext) -> Option<ValueIR> {
  let mut inner = pair.into_inner();
  let mut left = emit_expr_value(inner.next()?, func, module)?;
  while let Some(op) = inner.next() {
    let right = emit_expr_value(inner.next()?, func, module)?;
    let op_name = match op.as_rule() {
      Rule::multiply => "mul",
      Rule::divide => "sdiv",
      Rule::modulo => "srem",
      _ => return None,
    };
    let tmp = func.new_temp();
    func.emit(format!("{} = {} {} {}, {}", tmp, op_name, left.ty, left.val, right.val));
    left = ValueIR { ty: left.ty, val: tmp };
  }
  Some(left)
}

fn emit_comparison(pair: pest::iterators::Pair<Rule>, func: &mut FunctionContext, module: &mut ModuleContext) -> Option<ValueIR> {
  let mut inner = pair.into_inner();

  // Get the first child
  let first = inner.next()?;

  // Check if there's a second child (the operator)
  let op_pair = inner.next();
  if op_pair.is_none() {
    // No operator at this level - just pass through to the child
    return emit_expr_value(first, func, module);
  }

  // We have an operator, so this is an actual comparison/logical operation
  let op_pair = op_pair.unwrap();

  // Evaluate the left side
  let left = emit_expr_value(first, func, module)?;

  // Get the right side
  let right_pair = inner.next()?;
  let right = emit_expr_value(right_pair, func, module)?;

  // Check if this is a comparison or logical operation
  match op_pair.as_rule() {
    Rule::comparator => {
      let op = op_pair.as_str();
      let pred = match op {
        "==" => "eq",
        "!=" => "ne",
        "<" => "slt",
        "<=" => "sle",
        ">" => "sgt",
        ">=" => "sge",
        _ => "eq",
      };
      let tmp = func.new_temp();
      func.emit(format!("{} = icmp {} {} {}, {}", tmp, pred, left.ty, left.val, right.val));
      Some(ValueIR { ty: "i1".to_string(), val: tmp })
    },
    Rule::logical_and => {
      // TODO: Implement && properly with short-circuit evaluation
      Some(left)  // Placeholder
    },
    Rule::logical_or => {
      // TODO: Implement || properly with short-circuit evaluation
      Some(left)  // Placeholder
    },
    _ => None
  }
}

fn emit_circuit_var(pair: pest::iterators::Pair<Rule>, func: &mut FunctionContext, module: &mut ModuleContext) -> Option<ValueIR> {
  let arms: Vec<_> = pair.into_inner().collect();
  if arms.is_empty() {
    return None;
  }

  let end_label = func.new_label("circuit.end");
  let check_labels: Vec<String> = (0..arms.len())
    .map(|_| func.new_label("circuit.check"))
    .collect();
  let arm_labels: Vec<String> = (0..arms.len())
    .map(|_| func.new_label("circuit.arm"))
    .collect();
  let mut incoming: Vec<(String, String)> = Vec::new();
  let mut end_label_used = false;

  for (idx, arm) in arms.into_iter().enumerate() {
    let mut inner = arm.into_inner();
    let first = inner.next()?;
    let second = inner.next();
    let (cond_pair, value_pair) = if let Some(value) = second {
      (Some(first), value)
    } else {
      (None, first)
    };

    let value_pair = if value_pair.as_rule() == Rule::circuit_value {
      value_pair.into_inner().next()?
    } else {
      value_pair
    };

    let check_label = check_labels[idx].clone();
    let arm_label = arm_labels[idx].clone();
    let next_label = check_labels.get(idx + 1).cloned().unwrap_or_else(|| {
      end_label_used = true;
      end_label.clone()
    });

    // Emit the check block (skip label for first check to avoid back-to-back labels)
    if idx > 0 {
      func.emit(format!("{}:", check_label));
    }

    if let Some(cond_pair) = cond_pair {
      // Emit condition check and branch
      if let Some(cond) = emit_expr_value(cond_pair, func, module) {
        func.emit(format!("br i1 {}, label %{}, label %{}", cond.val, arm_label, next_label));
      }
    } else {
      // Default arm (wildcard) - unconditional jump
      func.emit(format!("br label %{}", arm_label));
    }

    // Emit the arm body
    func.emit(format!("{}:", arm_label));

    // Check if this is a block with return statement and evaluate accordingly
    let is_block_with_return = value_pair.as_rule() == Rule::block;
    let arm_value = if is_block_with_return {
      let ret_ty = func.ret_ty.clone();
      emit_circuit_block_value(value_pair, &ret_ty, func, module)
    } else {
      // Simple value expression
      emit_expr_value(value_pair, func, module)
    };

    // Always add to incoming - use a default value of 0 if evaluation failed
    let result_val = arm_value.map(|v| v.val).unwrap_or_else(|| "0".to_string());
    incoming.push((result_val, arm_label.clone()));

    // All arms branch to the end label
    func.emit(format!("br label %{}", end_label));
    end_label_used = true;
  }

  // Only emit end block if we have non-terminating arms
  if !incoming.is_empty() {
    func.emit(format!("{}:", end_label));
    let tmp = func.new_temp();
    let phi_list = incoming
      .iter()
      .map(|(val, label)| format!("[ {}, %{} ]", val, label))
      .collect::<Vec<_>>()
      .join(", ");
    func.emit(format!("{} = phi i64 {}", tmp, phi_list));
    Some(ValueIR { ty: "i64".to_string(), val: tmp })
  } else if end_label_used {
    // No values produced, but control can reach end_label
    func.emit(format!("{}:", end_label));
    func.emit("unreachable".to_string());
    None
  } else {
    // All arms terminated, no value to return
    None
  }
}

fn emit_circuit_value(pair: pest::iterators::Pair<Rule>, func: &mut FunctionContext, module: &mut ModuleContext) -> Option<ValueIR> {
  match pair.as_rule() {
    Rule::block => {
      for stmt in pair.into_inner() {
        if stmt.as_rule() == Rule::return_stmt {
          let mut inner = stmt.into_inner();
          if let Some(expr) = inner.next() {
            return emit_expr_value(expr, func, module);
          }
        }
      }
      None
    }
    _ => emit_expr_value(pair, func, module),
  }
}

fn emit_circuit_block_value(
  pair: pest::iterators::Pair<Rule>,
  ret_ty: &str,
  func: &mut FunctionContext,
  module: &mut ModuleContext,
) -> Option<ValueIR> {
  let stmts: Vec<_> = pair.into_inner().collect();
  let mut last_value: Option<ValueIR> = None;

  for (idx, stmt) in stmts.iter().enumerate() {
    let inner_pair = if stmt.as_rule() == Rule::statement {
      let mut inner = stmt.clone().into_inner();
      match inner.next() {
        Some(p) => p,
        None => continue,
      }
    } else {
      stmt.clone()
    };

    if inner_pair.as_rule() == Rule::return_stmt {
      let mut inner = inner_pair.into_inner();
      if let Some(expr) = inner.next() {
        return emit_expr_value(expr, func, module);
      }
      return None;
    }

    // Check if this is the last statement and it's an expression
    if idx == stmts.len() - 1 && inner_pair.as_rule() == Rule::expr {
      // Evaluate the last expression as the block's value
      last_value = emit_expr_value(inner_pair, func, module);
    } else {
      let _ = emit_statement_ir_with_return(inner_pair, ret_ty, func, module, false);
    }
  }

  last_value
}

fn expand_type_choices(choices: &[TypeChoice]) -> Vec<Vec<String>> {
  let mut results: Vec<Vec<String>> = Vec::new();

  fn expand_at(idx: usize, choices: &[TypeChoice], current: &mut Vec<String>, out: &mut Vec<Vec<String>>) {
    if idx >= choices.len() {
      out.push(current.clone());
      return;
    }

    match &choices[idx] {
      TypeChoice::Single(t) => {
        current.push(t.clone());
        expand_at(idx + 1, choices, current, out);
        current.pop();
      }
      TypeChoice::Variant(list, _) => {
        for t in list {
          current.push(t.clone());
          expand_at(idx + 1, choices, current, out);
          current.pop();
        }
      }
    }
  }

  expand_at(0, choices, &mut Vec::new(), &mut results);
  results
}

fn mangle_name(base: &str, types: &[String]) -> String {
  let suffix = types.join("__");
  format!("_minis__{}__{}", base, suffix)
}

fn mangle_method_name(class_name: &str, method_name: &str) -> String {
  format!("{}_{}", class_name, method_name)
}

fn extern_decl_name(decl: &str) -> Option<String> {
  let at_pos = decl.find('@')?;
  let rest = &decl[at_pos + 1..];
  let end = rest.find('(')?;
  Some(rest[..end].to_string())
}

fn escape_llvm_string(s: &str) -> (String, usize) {
  let mut escaped = String::new();
  for &b in s.as_bytes() {
    match b {
      b'"' => escaped.push_str("\\22"),
      b'\\' => escaped.push_str("\\5C"),
      b'\n' => escaped.push_str("\\0A"),
      b'\t' => escaped.push_str("\\09"),
      b'\r' => escaped.push_str("\\0D"),
      32..=126 => escaped.push(b as char),
      _ => escaped.push_str(&format!("\\{:02X}", b)),
    }
  }
  escaped.push_str("\\00");
  (escaped, s.as_bytes().len() + 1)
}

fn gep_for_string(global: &str, len: usize) -> String {
  format!("bitcast ([{} x i8]* @{} to i8*)", len, global)
}

fn parse_type_name(pair: pest::iterators::Pair<Rule>) -> Option<String> {
  match pair.as_rule() {
    Rule::type_spec => {
      let mut inner = pair.into_inner();
      while let Some(next) = inner.next() {
        if next.as_rule() == Rule::base_type {
          return Some(next.as_str().to_string());
        }
        if next.as_rule() == Rule::class_name {
          return Some(format!("%{}*", next.as_str()));
        }
      }
      None
    }
    Rule::base_type => Some(pair.as_str().to_string()),
    Rule::class_name => Some(format!("%{}*", pair.as_str())),
    Rule::type_ref => {
      let mut inner = pair.into_inner();
      parse_type_name(inner.next()?)
    }
    _ => None,
  }
}

fn print_pairs(pairs: Pairs<Rule>, indent: usize, input: &str) {
  for pair in pairs {
    let rule = pair.as_rule();
    let inner = pair.clone().into_inner();
    let child_count = inner.clone().count();

    // Skip pure pass-through nodes (single child with same span)
    if should_skip_rule(&rule, child_count, &pair) {
      print_pairs(pair.into_inner(), indent, input);
      continue;
    }

    let span = pair.as_span();
    let text = span.as_str();
    let snippet = summarize_text(text);

    // Rule-specific handling: concat string/int literals in list/expr lists
    if rule == Rule::list_literal {
      if let Some(concat) = concat_list_literal(&pair) {
        let display = format!("[\"{}\"]", concat);
        println!("{}{:?}  [{}..{}]  {}", "  ".repeat(indent), rule, span.start(), span.end(), display);
        continue; // Skip printing children
      }
    }

    if rule == Rule::expr_list {
      if let Some(concat) = concat_expr_list(&pair) {
        let display = format!("\"{}\"", concat);
        println!("{}{:?}  [{}..{}]  {}", "  ".repeat(indent), rule, span.start(), span.end(), display);
        continue; // Skip printing children
      }
    }

    // Highlight recovery rules
    match rule {
      _ => {
        println!("{}{:?}  [{}..{}]  {}", "  ".repeat(indent), rule, span.start(), span.end(), snippet);
      }
    }

    if child_count > 0 {
      print_pairs(pair.into_inner(), indent + 1, input);
    }
  }
}

fn literal_text(pair: &pest::iterators::Pair<Rule>) -> Option<String> {
  match pair.as_rule() {
    Rule::string => {
      let stripped = pair.as_str().trim_matches('"');
      let unescaped = stripped
        .replace("\\n", "\n")
        .replace("\\t", "\t")
        .replace("\\r", "\r");
      Some(unescaped)
    }
    Rule::number | Rule::hex_number | Rule::binary_number => Some(pair.as_str().to_string()),
    Rule::expr |
    Rule::logical_expr |
    Rule::logical_and_expr |
    Rule::comparison |
    Rule::arith_expr |
    Rule::term |
    Rule::primary |
    Rule::primary_base => {
      let mut inner = pair.clone().into_inner();
      let first = inner.next()?;
      if inner.next().is_none() {
        literal_text(&first)
      } else {
        None
      }
    }
    _ => None,
  }
}

fn concat_expr_list(pair: &pest::iterators::Pair<Rule>) -> Option<String> {
  if pair.as_rule() != Rule::expr_list {
    return None;
  }

  let children: Vec<_> = pair.clone().into_inner().collect();
  if children.is_empty() {
    return None;
  }

  let mut parts: Vec<String> = Vec::new();
  for child in &children {
    if let Some(text) = literal_text(child) {
      parts.push(text);
    } else {
      return None;
    }
  }

  Some(parts.join(""))
}

fn concat_list_literal(pair: &pest::iterators::Pair<Rule>) -> Option<String> {
  if pair.as_rule() != Rule::list_literal {
    return None;
  }

  let mut inner = pair.clone().into_inner();
  let expr_list = inner.next()?;
  if inner.next().is_some() {
    return None;
  }

  concat_expr_list(&expr_list)
}

fn get_line_col(input: &str, pos: usize) -> (usize, usize) {
  let mut line = 1;
  let mut col = 1;

  for (i, ch) in input.chars().enumerate() {
    if i >= pos {
      break;
    }
    if ch == '\n' {
      line += 1;
      col = 1;
    } else {
      col += 1;
    }
  }

  (line, col)
}

fn should_skip_rule(rule: &Rule, child_count: usize, pair: &pest::iterators::Pair<Rule>) -> bool {
  // Always skip statement wrappers (they're purely organizational)
  match rule {
    Rule::statement | Rule::top_statement => return true,
    _ => {}
  }

  // Always skip these wrapper nodes when they have a single child
  if child_count == 1 {
    match rule {
      Rule::primary |
      Rule::primary_base |
      Rule::param_list |
      Rule::type_ref |
      Rule::postfix |
      Rule::index_tail |
      Rule::member_tail => return true,
      _ => {}
    }
  }

  // Skip expression pass-throughs only when spans match (same content)
  if child_count == 1 {
    match rule {
      Rule::expr |
      Rule::logical_expr |
      Rule::logical_and_expr |
      Rule::comparison |
      Rule::arith_expr |
      Rule::term => {
        let inner = pair.clone().into_inner().next().unwrap();
        return pair.as_span() == inner.as_span();
      }
      _ => false
    }
  } else {
    false
  }
}

fn summarize_text(text: &str) -> String {
  // Replace escape sequences with actual characters
  let mut cleaned = text.replace("\\n", "\n").replace("\\t", "\t").replace("\\r", "\r");

  // Don't collapse whitespace if we have newlines
  if !cleaned.contains('\n') {
    cleaned = cleaned.replace('\n', " ").replace('\r', " ").replace('\t', " ");
    cleaned = cleaned.split_whitespace().collect::<Vec<_>>().join(" ");
  }

  const MAX: usize = 60;
  if cleaned.len() > MAX {
    format!("\"{}...\"", &cleaned[..MAX])
  } else {
    format!("\"{}\"", cleaned)
  }
}

fn show_multiline(text: &str) -> String {
  let lines: Vec<&str> = text.lines().collect();
  if lines.len() <= 3 {
    text.to_string()
  } else {
    format!("{}\n       ... ({} more lines) ...\n       {}",
      lines[0], lines.len() - 2, lines[lines.len() - 1])
  }
}

fn diagnose_error(text: &str) -> String {
  let trimmed = text.trim();

  // Check for common patterns
  if trimmed.starts_with("class ") {
    "Looks like a class declaration - check class syntax (class Name { public {...} })"
  } else if trimmed.contains("(") && trimmed.contains(")") && trimmed.contains("{") {
    "Looks like a function declaration - check return type, parameters, and block syntax"
  } else if trimmed.contains("=") && trimmed.contains("{") && trimmed.contains("->") {
    "Looks like circuit variable assignment - check circuit syntax ({ (condition) -> value; })"
  } else if trimmed.starts_with("for ") || trimmed.starts_with("while ") || trimmed.starts_with("if ") {
    "Control flow statement - check condition syntax and block delimiters"
  } else if trimmed.contains("=") {
    "Assignment or declaration - check type and expression syntax"
  } else if trimmed.starts_with(";") {
    "Unexpected semicolon - might be leftover from previous parse error"
  } else if trimmed == "}" {
    "Stray closing brace - check for mismatched braces or missing statement before it"
  } else {
    "Unknown syntax - check for typos, missing semicolons, or unsupported language features"
  }.to_string()
}

fn diagnose_block_error(text: &str) -> String {
  let trimmed = text.trim();

  if trimmed.starts_with("class ") {
    // Check for class syntax issues
    if !trimmed.contains("public {") && !trimmed.contains("private {") {
      "Class must have at least one access section (public { ... } or private { ... })".to_string()
    } else if trimmed.matches('{').count() != trimmed.matches('}').count() {
      format!("Mismatched braces: {} opening {{ vs {} closing }}",
        trimmed.matches('{').count(), trimmed.matches('}').count())
    } else {
      // Check for method/field syntax inside access sections
      let lines: Vec<&str> = trimmed.lines().collect();
      if lines.iter().any(|l| l.contains("->") && !l.trim().starts_with("//")) {
        "Circuit variable syntax (-> arrows) found - may need to check circuit arm formatting"
      } else {
        "Class declaration syntax error - check access modifiers, field types, and method signatures"
      }.to_string()
    }
  } else if trimmed.contains("public {") || trimmed.contains("private {") {
    "Access modifier block - should only appear inside class declarations".to_string()
  } else if trimmed.starts_with(";") {
    "Statement starts with semicolon - likely parse error continuation from previous line".to_string()
  } else if trimmed == "}" {
    "Unexpected closing brace - check block nesting and statement syntax".to_string()
  } else if trimmed.contains("->") && trimmed.contains("{") {
    "Circuit arm syntax - check if parent is a circuit variable ({ (cond) -> value; })".to_string()
  } else {
    diagnose_error(text)
  }
}

fn preprocess_macros(input: &str) -> String {
  let (stripped, macros) = extract_macros(input);
  let mut current = stripped;

  for _ in 0..5 {
    let next = expand_macros(&current, &macros);
    if next == current {
      break;
    }
    current = next;
  }

  current
}

fn extract_macros(input: &str) -> (String, HashMap<String, MacroDef>) {
  let mut macros: HashMap<String, MacroDef> = HashMap::new();
  let mut output = String::new();
  let lines: Vec<&str> = input.lines().collect();
  let mut i = 0;
  let mut include_stack: Vec<bool> = vec![true];

  while i < lines.len() {
    let line = lines[i];
    let trimmed = line.trim_start();

    if trimmed.starts_with("#if") {
      let include_parent = *include_stack.last().unwrap_or(&true);
      let ident = trimmed[3..].trim();
      let is_defined = macros.contains_key(ident);
      include_stack.push(include_parent && is_defined);
      i += 1;
      continue;
    }

    if trimmed.starts_with("#endif") {
      if include_stack.len() > 1 {
        include_stack.pop();
      }
      i += 1;
      continue;
    }

    if !*include_stack.last().unwrap_or(&true) {
      i += 1;
      continue;
    }

    if trimmed.starts_with("#def") {
      let (name, params, variadic, body, consumed) = parse_macro_def(&lines, i);
      if !name.is_empty() {
        macros.insert(
          name,
          MacroDef {
            params,
            variadic,
            body,
          },
        );
        i += consumed.max(1);
        continue;
      }
    }

    output.push_str(line);
    output.push('\n');
    i += 1;
  }

  (output, macros)
}

fn parse_macro_def(lines: &[&str], start: usize) -> (String, Vec<String>, bool, String, usize) {
  let line = lines[start];
  let trimmed = line.trim_start();
  let mut rest = trimmed.strip_prefix("#def").unwrap_or("").trim_start();

  let mut name = String::new();
  let mut idx = 0;
  if let Some(ch) = rest.chars().next() {
    if is_ident_start(ch) {
      for c in rest.chars() {
        if is_ident_continue(c) {
          name.push(c);
          idx += c.len_utf8();
        } else {
          break;
        }
      }
    }
  }

  rest = rest.get(idx..).unwrap_or("").trim_start();
  let mut params: Vec<String> = Vec::new();
  let mut variadic = false;

  if rest.starts_with('(') {
    if let Some(end) = find_matching_paren(rest) {
      let param_str = &rest[1..end];
      for raw in param_str.split(',') {
        let param = raw.trim();
        if param.is_empty() {
          continue;
        }
        if param == "$$*" {
          variadic = true;
        } else {
          params.push(param.to_string());
        }
      }
      rest = rest[end + 1..].trim_start();
    }
  }

  let mut body_lines: Vec<String> = Vec::new();
  let mut i = start;
  let mut current = rest.to_string();

  loop {
    let (line_content, continued) = strip_line_continuation(&current);
    body_lines.push(line_content);
    if continued {
      i += 1;
      if i >= lines.len() {
        break;
      }
      current = lines[i].to_string();
      continue;
    }
    break;
  }

  let body = body_lines.join("\n").trim_end().to_string();
  let consumed = i - start + 1;

  (name, params, variadic, body, consumed)
}

fn strip_line_continuation(line: &str) -> (String, bool) {
  let mut trimmed = line.trim_end().to_string();
  if trimmed.ends_with('\\') {
    trimmed.pop();
    (trimmed.trim_end().to_string(), true)
  } else {
    (line.to_string(), false)
  }
}

fn find_matching_paren(s: &str) -> Option<usize> {
  let mut depth = 0;
  let mut in_string = false;
  for (i, ch) in s.chars().enumerate() {
    if ch == '"' {
      in_string = !in_string;
    }
    if in_string {
      continue;
    }
    if ch == '(' {
      depth += 1;
    } else if ch == ')' {
      depth -= 1;
      if depth == 0 {
        return Some(i);
      }
    }
  }
  None
}

fn expand_macros(input: &str, macros: &HashMap<String, MacroDef>) -> String {
  let mut out = String::new();
  let mut i = 0;
  let chars: Vec<char> = input.chars().collect();
  let mut in_string = false;

  while i < chars.len() {
    let ch = chars[i];
    if ch == '"' {
      in_string = !in_string;
      out.push(ch);
      i += 1;
      continue;
    }

    if !in_string && is_ident_start(ch) {
      let start = i;
      let mut end = i + 1;
      while end < chars.len() && is_ident_continue(chars[end]) {
        end += 1;
      }
      let ident: String = chars[start..end].iter().collect();

      if let Some(def) = macros.get(&ident) {
        let (next_i, replacement) = expand_macro_at(input, &chars, end, def);
        if let Some(repl) = replacement {
          out.push_str(&repl);
          i = next_i;
          continue;
        }
      }

      out.push_str(&ident);
      i = end;
      continue;
    }

    out.push(ch);
    i += 1;
  }

  out
}

fn expand_macro_at(
  input: &str,
  chars: &[char],
  ident_end: usize,
  def: &MacroDef,
) -> (usize, Option<String>) {
  let mut i = ident_end;
  while i < chars.len() && chars[i].is_whitespace() {
    i += 1;
  }

  if i < chars.len() && chars[i] == '(' {
    if let Some((args, end_idx)) = parse_call_args(input, i) {
      let replaced = apply_macro(def, &args);
      let mut next_i = end_idx;
      if replaced.trim_end().ends_with(';') {
        while next_i < chars.len() && chars[next_i].is_whitespace() {
          next_i += 1;
        }
        if next_i < chars.len() && chars[next_i] == ';' {
          next_i += 1;
        }
      }
      return (next_i, Some(replaced));
    }
  }

  if def.params.is_empty() && !def.variadic {
    return (ident_end, Some(def.body.clone()));
  }

  (ident_end, None)
}

fn parse_call_args(input: &str, start_paren: usize) -> Option<(Vec<String>, usize)> {
  let chars: Vec<char> = input.chars().collect();
  let mut depth = 0;
  let mut in_string = false;
  let mut args: Vec<String> = Vec::new();
  let mut current = String::new();
  let mut i = start_paren;

  while i < chars.len() {
    let ch = chars[i];
    if ch == '"' {
      in_string = !in_string;
    }
    if !in_string {
      if ch == '(' {
        depth += 1;
        if depth > 1 {
          current.push(ch);
        }
        i += 1;
        continue;
      }
      if ch == ')' {
        depth -= 1;
        if depth == 0 {
          let trimmed = current.trim();
          if !trimmed.is_empty() {
            args.push(trimmed.to_string());
          }
          return Some((args, i + 1));
        }
        current.push(ch);
        i += 1;
        continue;
      }
      if ch == ',' && depth == 1 {
        args.push(current.trim().to_string());
        current.clear();
        i += 1;
        continue;
      }
    }
    if depth >= 1 {
      current.push(ch);
    }
    i += 1;
  }

  None
}

fn apply_macro(def: &MacroDef, args: &[String]) -> String {
  let mut body = def.body.clone();
  let mut named_args: HashMap<String, String> = HashMap::new();

  for (idx, param) in def.params.iter().enumerate() {
    let value = args.get(idx).cloned().unwrap_or_default();
    named_args.insert(param.clone(), value);
  }

  let varargs = if def.variadic && args.len() >= def.params.len() {
    args[def.params.len()..].to_vec()
  } else {
    Vec::new()
  };
  let var_list = format!("[{}]", varargs.join(", "));

  body = replace_token_outside_strings(&body, "$$*", &var_list);
  for (param, value) in named_args {
    body = replace_identifier_outside_strings(&body, &param, &value);
  }

  body
}

fn replace_token_outside_strings(input: &str, token: &str, replacement: &str) -> String {
  let mut out = String::new();
  let mut i = 0;
  let chars: Vec<char> = input.chars().collect();
  let token_chars: Vec<char> = token.chars().collect();
  let mut in_string = false;

  while i < chars.len() {
    let ch = chars[i];
    if ch == '"' {
      in_string = !in_string;
      out.push(ch);
      i += 1;
      continue;
    }
    if !in_string && matches_at(&chars, i, &token_chars) {
      out.push_str(replacement);
      i += token_chars.len();
      continue;
    }
    out.push(ch);
    i += 1;
  }

  out
}

fn replace_identifier_outside_strings(input: &str, ident: &str, replacement: &str) -> String {
  let mut out = String::new();
  let chars: Vec<char> = input.chars().collect();
  let mut i = 0;
  let mut in_string = false;

  while i < chars.len() {
    let ch = chars[i];
    if ch == '"' {
      in_string = !in_string;
      out.push(ch);
      i += 1;
      continue;
    }
    if !in_string && is_ident_start(ch) {
      let start = i;
      let mut end = i + 1;
      while end < chars.len() && is_ident_continue(chars[end]) {
        end += 1;
      }
      let token: String = chars[start..end].iter().collect();
      if token == ident {
        out.push_str(replacement);
      } else {
        out.push_str(&token);
      }
      i = end;
      continue;
    }
    out.push(ch);
    i += 1;
  }

  out
}

fn matches_at(haystack: &[char], start: usize, needle: &[char]) -> bool {
  if start + needle.len() > haystack.len() {
    return false;
  }
  for (i, ch) in needle.iter().enumerate() {
    if haystack[start + i] != *ch {
      return false;
    }
  }
  true
}

fn is_ident_start(ch: char) -> bool {
  ch == '_' || ch.is_ascii_alphabetic()
}

fn is_ident_continue(ch: char) -> bool {
  ch == '_' || ch.is_ascii_alphanumeric()
}
