// AST (Abstract Syntax Tree) definitions for Vedit

#[derive(Debug, Clone, PartialEq)]
pub enum Type {
    Int,
    Float,
    String,
    Bool,
    Tribool,
    List(Box<Type>),
    Void,
    Unknown,
}

#[derive(Debug, Clone)]
pub struct TypeSpec {
    pub is_const: bool,
    pub is_static: bool,
    pub base_type: Type,
}

#[derive(Debug, Clone)]
pub enum Expr {
    Number(f64),
    String(String),
    Bool(bool),
    Identifier(String),
    FunctionCall {
        name: String,
        args: Vec<Expr>,
    },
    MethodCall {
        object: String,
        method: String,
        args: Vec<Expr>,
    },
    BinaryOp {
        left: Box<Expr>,
        op: BinaryOperator,
        right: Box<Expr>,
    },
    List(Vec<Expr>),
    Range {
        start: i64,
        end: i64,
        step: Option<i64>,
    },
}

#[derive(Debug, Clone, PartialEq)]
pub enum BinaryOperator {
    Add,
    Subtract,
    Multiply,
    Divide,
    Modulo,
}

#[derive(Debug, Clone)]
pub enum Statement {
    VarDecl {
        type_spec: TypeSpec,
        name: String,
        value: Expr,
    },
    ListDecl {
        type_spec: Option<TypeSpec>,
        name: String,
        values: Vec<Expr>,
    },
    FuncDecl {
        name: String,
        params: Vec<(TypeSpec, String)>,
        body: Block,
    },
    Assignment {
        name: String,
        value: Expr,
    },
    PropertyAssignment {
        object: String,
        property: String,
        value: Expr,
    },
    FunctionCall {
        name: String,
        args: Vec<Expr>,
    },
    MethodCall {
        object: String,
        method: String,
        args: Vec<Expr>,
    },
    If {
        condition: Expr,
        then_block: Block,
        else_block: Option<Block>,
    },
    While {
        condition: Expr,
        body: Block,
    },
    Try {
        try_block: Block,
        catch_var: String,
        catch_block: Block,
    },
    For {
        var: Option<String>,  // Loop variable (optional)
        range: (i64, i64, Option<i64>), // start, end, step
        body: Block,
    },
    Block(Block),
    Return(Option<Expr>),
}

#[derive(Debug, Clone)]
pub struct Block {
    pub statements: Vec<Statement>,
}

impl Block {
    pub fn new() -> Self {
        Block {
            statements: Vec::new(),
        }
    }

    pub fn is_empty(&self) -> bool {
        self.statements.is_empty()
    }

    /// Check if this block only contains other blocks (for optimization)
    pub fn is_only_blocks(&self) -> bool {
        !self.statements.is_empty() &&
        self.statements.iter().all(|s| matches!(s, Statement::Block(_)))
    }
}

#[derive(Debug)]
pub struct Program {
    pub statements: Vec<Statement>,
}
