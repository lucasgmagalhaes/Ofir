# Appendix A. Grammar

This is the complete syntactic grammar of Astra in EBNF. Lexical tokens
(identifiers, literals, keywords, operators) are defined in chapter 1.

`NL` is the statement terminator inserted by the lexer at the end of a line
(chapter 1.3). Where the grammar allows `NL`, any number of consecutive
terminators may appear. A terminator before `}` may be omitted.

## A.1 Files and items

```ebnf
File          = [ ModuleDoc ] { Import } { Item } EOF .
Import        = "import" ModulePath [ "as" identifier ] NL .
ModulePath    = identifier { "." identifier } .

Item          = { Attribute } [ "pub" ] ItemKind .
ItemKind      = FnDecl
              | StructDecl
              | EnumDecl
              | InterfaceDecl
              | ConstDecl
              | GlobalDecl
              | TypeAlias
              | ExternBlock .

Attribute     = "@" identifier [ "(" [ AttrArg { "," AttrArg } ] ")" ] [ NL ] .
AttrArg       = Literal | identifier | AttrCond .
AttrCond      = Expr .                       (* restricted to @cfg conditions *)
```

## A.2 Functions

```ebnf
FnDecl        = [ "unsafe" ] [ "extern" string_lit ] "fn" FnName
                [ TypeParams ] "(" [ Params ] ")" [ "->" Type ]
                [ WhereClause ] Block .

FnName        = Receiver identifier             (* method *)
              | TypePath [ TypeArgs ] "." identifier   (* associated function *)
              | identifier .                     (* free function *)

Receiver      = "(" identifier ":" ReceiverType ")" .
ReceiverType  = [ "&" [ "mut" ] ] TypePath [ TypeArgs ] .

Params        = Param { "," Param } [ "," ] .
Param         = identifier ":" Type .

TypeParams    = "<" TypeParam { "," TypeParam } [ "," ] ">" .
TypeParam     = identifier [ ":" Bounds ] .
Bounds        = TypeRef { "+" TypeRef } .
TypeRef       = TypePath [ TypeArgs ] .
WhereClause   = "where" WherePred { "," WherePred } [ "," ] .
WherePred     = Type ":" Bounds .
```

## A.3 Types declarations

```ebnf
StructDecl    = [ "copy" | "ref" ] "struct" identifier [ TypeParams ]
                [ ":" Bounds ] "{" { FieldDecl } "}" .
FieldDecl     = { Attribute } [ "pub" ] identifier ":" Type ( "," | NL ) .

EnumDecl      = [ "copy" ] "enum" identifier [ TypeParams ]
                [ ":" Bounds ] "{" { Variant } "}" .
Variant       = identifier [ "(" VariantField { "," VariantField } [ "," ] ")" ]
                [ "=" int_lit ] ( "," | NL ) .
VariantField  = [ identifier ":" ] Type .

InterfaceDecl = "interface" identifier [ TypeParams ] "{" { IfaceMember } "}" .
IfaceMember   = TypeRef NL                                 (* embedding *)
              | "fn" IfaceRecv identifier [ TypeParams ]
                "(" [ Params ] ")" [ "->" Type ] [ WhereClause ]
                ( Block | NL ) .
IfaceRecv     = "(" [ identifier ":" ] [ "&" [ "mut" ] ] "Self" ")"
              | "Self" "." .

ConstDecl     = "const" identifier [ ":" Type ] "=" Expr NL .
GlobalDecl    = "var" identifier ":" Type "=" Expr NL .
TypeAlias     = "type" identifier [ TypeParams ] "=" Type NL .

ExternBlock   = "extern" string_lit "{" { { Attribute } FnSignature NL } "}" .
FnSignature   = "fn" identifier "(" [ Params ] ")" [ "->" Type ] .
```

## A.4 Types

```ebnf
Type          = TypePath [ TypeArgs ]
              | "Self"
              | "&" [ "mut" ] Type
              | "*" [ "mut" ] Type
              | "[" Type [ ";" Expr ] "]"       (* slice element or fixed array *)
              | "(" [ Type { "," Type } [ "," ] ] ")"   (* unit or tuple *)
              | "fn" "(" [ Type { "," Type } ] ")" [ "->" Type ]
              | "dyn" Bounds
              | Type "?"                        (* Option<Type> *)
              | "!" .

TypePath      = identifier { "." identifier } .
TypeArgs      = "<" Type { "," Type } [ "," ] ">" .
```

A one-element tuple type is written `(T,)`. `[T]` without a length is only
valid behind a reference (`&[T]`, `&mut [T]`).

## A.5 Statements

```ebnf
Block         = "{" { Statement } "}" .
Statement     = ( LetStmt
                | VarStmt
                | ConstDecl
                | AssignStmt
                | ForStmt
                | ReturnStmt
                | "break"
                | "continue"
                | ExprStmt ) [ NL ] .

LetStmt       = "let" Pattern [ ":" Type ] "=" Expr .
VarStmt       = "var" identifier [ ":" Type ] [ "=" Expr ] .
AssignStmt    = Expr AssignOp Expr .
AssignOp      = "=" | "+=" | "-=" | "*=" | "/=" | "%="
              | "&=" | "|=" | "^=" | "<<=" | ">>=" .
ForStmt       = "for" [ ForHeader ] Block .
ForHeader     = Expr                                       (* condition *)
              | identifier [ "," identifier ] "in" Expr .  (* iteration *)
ReturnStmt    = "return" [ Expr ] .
ExprStmt      = Expr .
```

## A.6 Expressions

```ebnf
Expr          = RangeExpr .
RangeExpr     = OrExpr [ ( ".." | "..=" ) OrExpr ] .
OrExpr        = AndExpr { "||" AndExpr } .
AndExpr       = CmpExpr { "&&" CmpExpr } .
CmpExpr       = AddExpr [ CmpOp AddExpr ] .              (* non-associative *)
CmpOp         = "==" | "!=" | "<" | "<=" | ">" | ">=" .
AddExpr       = MulExpr { ( "+" | "-" | "|" | "^" ) MulExpr } .
MulExpr       = CastExpr { ( "*" | "/" | "%" | "<<" | ">>" | "&" ) CastExpr } .
CastExpr      = UnaryExpr { "as" Type } .
UnaryExpr     = ( "-" | "!" | "*" | "&" [ "mut" ] ) UnaryExpr
              | PostfixExpr .

PostfixExpr   = Primary { Postfix } .
Postfix       = "(" [ Args ] ")"                 (* call *)
              | "[" Expr "]"                     (* index or slice *)
              | "." identifier                   (* field or method *)
              | "." int_lit                      (* tuple field *)
              | "?" .                            (* error propagation *)
Args          = Expr { "," Expr } [ "," ] .

Primary       = Literal
              | identifier
              | "Self"
              | TypePath TypeArgs                (* see 8.5 for disambiguation *)
              | "(" Expr ")"
              | "(" [ Expr "," [ Expr { "," Expr } [ "," ] ] ] ")"   (* tuple *)
              | ArrayLit
              | StructLit
              | "." identifier [ "(" [ Args ] ")" ]   (* enum variant shorthand *)
              | IfExpr
              | MatchExpr
              | Closure
              | Block
              | "unsafe" Block .

ArrayLit      = "[" [ Expr { "," Expr } [ "," ] ] "]"
              | "[" Expr ";" Expr "]" .
StructLit     = TypePath [ TypeArgs ] "{" [ FieldInit { Sep FieldInit } ]
                [ Sep ".." Expr ] [ Sep ] "}" .
FieldInit     = identifier [ ":" Expr ] .          (* `x` is short for `x: x` *)
Sep           = "," | NL .

IfExpr        = "if" Cond Block [ "else" ( IfExpr | Block ) ] .
Cond          = Expr | "let" Pattern "=" Expr .
MatchExpr     = "match" Expr "{" { MatchArm } "}" .
MatchArm      = Pattern [ "if" Expr ] "=>" ( Expr | Block ) ( "," | NL ) .

Closure       = [ "move" ] "fn" "(" [ ClosureParam { "," ClosureParam } ] ")"
                [ "->" Type ] ( Block | "=>" Expr ) .
ClosureParam  = identifier [ ":" Type ] .

Literal       = int_lit | float_lit | string_lit | byte_lit
              | "true" | "false" .
```

**Struct literal restriction.** In the condition of `if`, `match` and the
header of `for`, a struct literal must be parenthesized, as in Go:
`if p == (Point{x: 0, y: 0}) { }`. Otherwise the `{` is the start of the
block.

## A.7 Patterns

```ebnf
Pattern       = PatternAtom { "|" PatternAtom } .
PatternAtom   = "_"
              | [ "mut" ] identifier                    (* binding *)
              | Literal
              | [ "-" ] Literal ( ".." | "..=" ) [ "-" ] Literal
              | "." identifier [ "(" PatternList ")" ]  (* variant shorthand *)
              | TypePath "." identifier [ "(" PatternList ")" ]
              | TypePath "{" FieldPat { "," FieldPat } [ "," ".." ] "}"
              | "(" PatternList ")"                     (* tuple *)
              | identifier ":" Type .                   (* dyn type test *)
PatternList   = Pattern { "," Pattern } [ "," ] .
FieldPat      = identifier [ ":" Pattern ] .
```
