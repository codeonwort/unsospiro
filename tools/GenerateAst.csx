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
    "Binary : Expr left, Token operator, Expr right",
    "Grouping : Expr expression",
    "Literal : Object value",
    "Unary : Token operator, Expr right"
]);

void DefineAst(string outputDir, string baseName, string[] types)
{
    string path = $"{outputDir}/{baseName}.cs";
    StringBuilder writer = new StringBuilder();

    writer.AppendLine(
        $$"""
        namespace UnSospiro
        {
            abstract class {{baseName}}
            {
                //
            }
        }
        """);

    // TODO: Implement DefineType()

    string source = writer.ToString();
    File.WriteAllText(path, source);

    Console.WriteLine($"Written to: {path}");
}
