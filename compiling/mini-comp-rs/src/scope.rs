// scope.rs - Manages variable scopes
use hashbrown::HashMap;
use crate::ast::TypeSpec;
use crate::value::Value;

#[derive(Clone)]
pub struct Scope {
    parent: Option<Box<Scope>>,
    pub variables: HashMap<String, VarInfo>,
}

#[derive(Debug, Clone)]
pub struct VarInfo {
    pub name: String,
    pub type_spec: TypeSpec,
    pub value: Value,
}

impl Scope {
    pub fn new() -> Self {
        Scope {
            parent: None,
            variables: HashMap::new(),
        }
    }

    /// Create a child scope
    pub fn child(parent: Scope) -> Self {
        Scope {
            parent: Some(Box::new(parent)),
            variables: HashMap::new(),
        }
    }

    /// Declare a new variable in this scope
    /// If variable already exists in THIS scope (not parent), update it instead of erroring
    pub fn declare(&mut self, name: String, type_spec: TypeSpec, value: Value) -> Result<(), String> {
        // If variable exists in this scope, update it
        if let Some(existing) = self.variables.get_mut(&name) {
            // Type check
            if !value.matches_type(&type_spec.base_type) {
                return Err(format!(
                    "Type mismatch: variable '{}' expects {:?} but got {:?}",
                    name, type_spec.base_type, value.get_type()
                ));
            }
            existing.value = value;
            existing.type_spec = type_spec;
            return Ok(());
        }

        // Type check
        if !value.matches_type(&type_spec.base_type) {
            return Err(format!(
                "Type mismatch: variable '{}' expects {:?} but got {:?}",
                name, type_spec.base_type, value.get_type()
            ));
        }

        self.variables.insert(name.clone(), VarInfo {
            name,
            type_spec,
            value,
        });

        Ok(())
    }

    /// Get a variable from this scope or parent scopes
    /// NOTE: Global scope is checked via parent chain
    pub fn get(&self, name: &str) -> Option<&VarInfo> {
        if let Some(var) = self.variables.get(name) {
            Some(var)
        } else if let Some(parent) = &self.parent {
            parent.get(name)
        } else {
            None
        }
    }

    /// Update a variable value
    pub fn set(&mut self, name: &str, value: Value) -> Result<(), String> {
        if let Some(var) = self.variables.get_mut(name) {
            // Check if const
            if var.type_spec.is_const {
                return Err(format!("Cannot assign to const variable '{}'", name));
            }

            // Type check
            if !value.matches_type(&var.type_spec.base_type) {
                return Err(format!(
                    "Type mismatch: variable '{}' expects {:?} but got {:?}",
                    name, var.type_spec.base_type, value.get_type()
                ));
            }

            var.value = value;
            Ok(())
        } else if let Some(parent) = &mut self.parent {
            parent.set(name, value)
        } else {
            Err(format!("Variable '{}' not found", name))
        }
    }
}
