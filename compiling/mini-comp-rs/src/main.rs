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
      print_pairs(pairs, 0);
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

fn print_pairs(pairs: Pairs<Rule>, indent: usize) {
  for pair in pairs {
    let rule = pair.as_rule();
    let inner = pair.clone().into_inner();
    let child_count = inner.clone().count();

    // Skip pure pass-through nodes (single child with same span)
    if should_skip_rule(&rule, child_count, &pair) {
      print_pairs(pair.into_inner(), indent);
      continue;
    }

    let span = pair.as_span();
    let text = span.as_str();
    let snippet = summarize_text(text);
    println!("{}{:?}  [{}..{}]  {}", "  ".repeat(indent), rule, span.start(), span.end(), snippet);

    if child_count > 0 {
      print_pairs(pair.into_inner(), indent + 1);
    }
  }
}

fn should_skip_rule(rule: &Rule, child_count: usize, pair: &pest::iterators::Pair<Rule>) -> bool {
  // Skip expression wrapper layers that just pass through a single child
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
