// value.rs - Runtime values
use crate::ast::Type;

#[derive(Debug, Clone)]
pub enum Value {
    Int(i64),
    Float(f64),
    String(String),
    Bool(bool),
    List(Vec<Value>),
    Void,
}

impl Value {
    /// Check if this value matches the expected type
    pub fn matches_type(&self, expected: &Type) -> bool {
        match (self, expected) {
            (Value::Int(_), Type::Int) => true,
            (Value::Float(_), Type::Float) => true,
            (Value::String(_), Type::String) => true,
            (Value::Bool(_), Type::Bool) => true,
            (Value::List(_), Type::List(_)) => true,
            (_, Type::Unknown) => true, // Unknown type accepts anything
            // Auto-convert int to float
            (Value::Int(_), Type::Float) => true,
            _ => false,
        }
    }

    /// Get the type of this value
    pub fn get_type(&self) -> Type {
        match self {
            Value::Int(_) => Type::Int,
            Value::Float(_) => Type::Float,
            Value::String(_) => Type::String,
            Value::Bool(_) => Type::Bool,
            Value::List(items) => {
                if items.is_empty() {
                    Type::List(Box::new(Type::Unknown))
                } else {
                    Type::List(Box::new(items[0].get_type()))
                }
            }
            Value::Void => Type::Void,
        }
    }

    /// Convert to boolean for conditionals
    pub fn to_bool(&self) -> bool {
        match self {
            Value::Bool(b) => *b,
            Value::Int(i) => *i != 0,
            Value::Float(f) => *f != 0.0,
            Value::String(s) => !s.is_empty(),
            Value::List(l) => !l.is_empty(),
            Value::Void => false,
            _ => true,
        }
    }

    /// Display value as string
    pub fn to_string(&self) -> String {
        match self {
            Value::Int(i) => i.to_string(),
            Value::Float(f) => f.to_string(),
            Value::String(s) => s.clone(),
            Value::Bool(b) => b.to_string(),
            Value::List(items) => {
                let strs: Vec<String> = items.iter().map(|v| v.to_string()).collect();
                format!("[{}]", strs.join(", "))
            }
            Value::Void => "void".to_string(),
        }
    }
}
