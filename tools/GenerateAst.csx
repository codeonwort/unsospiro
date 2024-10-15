// How to run this script:
// 1. Open Developer Command Prompt for VS 2022.
// 2. Type 'csi GenerateAst.csx'.

using System;
using System.Text;

var cmdArgs = Environment.GetCommandLineArgs();

if (cmdArgs.Length != 3) // 3rd argument because 'csi GenerateAst.csx <output directory>'
{
    Console.WriteLine("Usage: GenerateAst.csx <output directory>");
    Environment.Exit(64);
}

string outputDir = cmdArgs[2];

DefineAst(outputDir, "Expr", [
    "Assign : Token name, Expr value",
    "Binary : Expr left, Token op, Expr right",
    "Call : Expr callee, Token paren, List<Expr> arguments",
    "Grouping : Expr expression",
    "Literal : Object value",
    "Logical : Expr left, Token op, Expr right",
    "Unary : Token op, Expr right",
    "Variable : Token name"
]);

DefineAst(outputDir, "Stmt", [
    "Block : List<Stmt> statements",
    "Class : Token name, List<Stmt.Function> methods",
    "Expression : Expr expression",
    "Function : Token name, List<Token> parameters, List<Stmt> body",
    "If : Expr condition, Stmt thenBranch, Stmt elseBranch",
    "Print : Expr expression",
    "Return : Token keyword, Expr value",
    "Var : Token name, Expr initializer",
    "While : Expr condition, Stmt body"
]);

// outputDir = "C:/some/path/"
// baseName  = "Expr"
// types     = ["Binary : Expr left, Token op, Expr right", "Grouping : Expr expression", ...]
void DefineAst(string outputDir, string baseName, string[] types)
{
    string path = $"{outputDir}/{baseName}.cs";
    StringBuilder writer = new StringBuilder();

    writer.AppendLine($"namespace UnSospiro");
    writer.AppendLine($"{{");
    writer.AppendLine($"\tinternal abstract class {baseName}");
    writer.AppendLine($"\t{{");
    DefineVisitor(writer, baseName, types);
    foreach (string type in types)
    {
        string[] splits = type.Split(':');
        string className = splits[0].Trim();
        string fieldList = splits[1].Trim();
        DefineType(writer, baseName, className, fieldList);
    }
    writer.AppendLine();
    writer.AppendLine($"\t\tinternal abstract R Accept<R>(Visitor<R> visitor);");
    writer.AppendLine($"\t}}");
    writer.AppendLine($"}}");

    string source = writer.ToString();
    File.WriteAllText(path, source);

    Console.WriteLine($"Written to: {path}");
}

void DefineVisitor(StringBuilder writer, string baseName, string[] types)
{
    writer.AppendLine($"\t\tinternal interface Visitor<R>");
    writer.AppendLine($"\t\t{{");
    foreach (string type in types)
    {
        string typeName = type.Split(':')[0].Trim();
        writer.AppendLine($"\t\t\tR Visit{typeName}{baseName}({typeName} {baseName.ToLower()});");
    }
    writer.AppendLine($"\t\t}}");
}

void DefineType(StringBuilder writer, string baseName, string className, string fieldList)
{
    string[] fields = fieldList.Split(',');

    writer.AppendLine($"\t\tinternal class {className} : {baseName}");
    writer.AppendLine($"\t\t{{");
    // Constructor
    writer.AppendLine($"\t\t\tpublic {className}({fieldList})");
    writer.AppendLine($"\t\t\t{{");
    foreach (string field in fields)
    {
        string fieldName = field.Trim().Split(' ')[1];
        writer.AppendLine($"\t\t\t\tthis.{fieldName} = {fieldName};");
    }
    writer.AppendLine($"\t\t\t}}");
    // Visitor pattern
    writer.AppendLine($"\t\t\tinternal override R Accept<R>(Visitor<R> visitor)");
    writer.AppendLine($"\t\t\t{{");
    writer.AppendLine($"\t\t\t\treturn visitor.Visit{className}{baseName}(this);");
    writer.AppendLine($"\t\t\t}}");
    // Field declarations
    foreach (string field in fields)
    {
        writer.AppendLine($"\t\t\tinternal {field.Trim()};");
    }
    writer.AppendLine($"\t\t}}");
}
