namespace UnSospiro
{
    internal class MainProgram
    {
        private static void Main(string[] args)
        {
            if (args.Length > 1)
            {
                Console.WriteLine("Usage: unsospiro [script]");
                Environment.Exit(64);
            }
            else if (args.Length == 1)
            {
                RunFile(args[0]);
            }
            else
            {
                RunPrompt();
            }
        }

        private static void RunFile(string file)
        {
            string content = File.ReadAllText(Path.GetFullPath(file), System.Text.Encoding.Default);
            Run(content);
        }

        private static void RunPrompt()
        {
            while (true)
            {
                Console.Write("> ");
                string? line = Console.ReadLine();
                if (line == null) break;
                Run(line);
            }
        }

        private static void Run(string content)
        {
            // TODO: Implement Scanner, scan tokens, and print the tokens.
            Console.WriteLine($"input: {content}");
        }
    }
}
