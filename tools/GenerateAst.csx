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
    "Binary : Expr left, Token op, Expr right",
    "Grouping : Expr expression",
    "Literal : Object value",
    "Unary : Token op, Expr right"
]);

void DefineAst(string outputDir, string baseName, string[] types)
{
    string path = $"{outputDir}/{baseName}.cs";
    StringBuilder writer = new StringBuilder();

    writer.AppendLine($"namespace UnSospiro");
    writer.AppendLine($"{{");
    writer.AppendLine($"\tinternal abstract class {baseName}");
    writer.AppendLine($"\t{{");
    foreach (string type in types)
    {
        string[] splits = type.Split(':');
        string className = splits[0].Trim();
        string fieldList = splits[1].Trim();
        DefineType(writer, baseName, className, fieldList);
    }
    writer.AppendLine($"\t}}");
    writer.AppendLine($"}}");

    string source = writer.ToString();
    File.WriteAllText(path, source);

    Console.WriteLine($"Written to: {path}");
}

void DefineType(StringBuilder writer, string baseName, string className, string fieldList)
{
    string[] fields = fieldList.Split(',');

    writer.AppendLine($"\t\tprivate class {className} : {baseName}");
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
    // Field declarations
    foreach (string field in fields)
    {
        writer.AppendLine($"\t\t\t{field.Trim()};");
    }
    writer.AppendLine($"\t\t}}");
}
