use std::env;
use std::fs;
use std::io::{self, Read};

use pest::Parser;
use pest::iterators::Pairs;
use pest_derive::Parser;

mod scope;
mod value;
mod ast;

#[derive(Parser)]
#[grammar = "grammar.pest"]
struct MiniParser;

fn main() {
  let input = read_input();

  match MiniParser::parse(Rule::program, &input) {
    Ok(pairs) => {
      println!("Parse ok. Tree:");
      print_pairs(pairs, 0, &input);
    }
    Err(err) => {
      eprintln!("Parse error:\n{}", err);
      std::process::exit(1);
    }
  }
}

fn read_input() -> String {
  let mut args = env::args().skip(1);

  if let Some(path) = args.next() {
    fs::read_to_string(path).unwrap_or_else(|err| {
      eprintln!("Failed to read input file: {err}");
      std::process::exit(1);
    })
  } else {
    let mut buf = String::new();
    io::stdin().read_to_string(&mut buf).unwrap_or_else(|err| {
      eprintln!("Failed to read stdin: {err}");
      std::process::exit(1);
    });
    buf
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

    // Highlight recovery rules
    match rule {
      Rule::error_recovery => {
        let (line, col) = get_line_col(input, span.start());
        let diagnosis = diagnose_error(text);
        eprintln!("\n{}🔴 ERROR RECOVERY at line {}:{}", "  ".repeat(indent), line, col);
        eprintln!("{}   Failed to parse: {}", "  ".repeat(indent), text);
        eprintln!("{}   Diagnosis: {}", "  ".repeat(indent), diagnosis);
        eprintln!("{}   Consumed up to semicolon [{}..{}]\n", "  ".repeat(indent), span.start(), span.end());
      }
      Rule::block_recovery => {
        let (line, col) = get_line_col(input, span.start());
        let diagnosis = diagnose_block_error(text);
        eprintln!("\n{}🔴 BLOCK RECOVERY at line {}:{}", "  ".repeat(indent), line, col);
        eprintln!("{}   Failed to parse: {}", "  ".repeat(indent), show_multiline(text));
        eprintln!("{}   Diagnosis: {}", "  ".repeat(indent), diagnosis);
        eprintln!("{}   Consumed up to closing brace [{}..{}]\n", "  ".repeat(indent), span.start(), span.end());
      }
      _ => {
        println!("{}{:?}  [{}..{}]  {}", "  ".repeat(indent), rule, span.start(), span.end(), snippet);
      }
    }

    if child_count > 0 {
      print_pairs(pair.into_inner(), indent + 1, input);
    }
  }
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
  
  // Skip wrapper layers that just pass through a single child
  if child_count == 1 {
    match rule {
      Rule::expr |
      Rule::logical_expr |
      Rule::logical_and_expr |
      Rule::comparison |
      Rule::arith_expr |
      Rule::term => {
        // Only skip if the child spans the same text (pure pass-through)
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
  let mut cleaned = text.replace('\n', " ").replace('\r', " ").replace('\t', " ");
  cleaned = cleaned.split_whitespace().collect::<Vec<_>>().join(" ");

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
