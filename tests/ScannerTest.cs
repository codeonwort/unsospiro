using Microsoft.VisualStudio.TestTools.UnitTesting;
using UnSospiro;

namespace Project.tests
{
    internal class MockListener : IErrorListener
    {
        private bool hadError = false;
        private bool hadRuntimeError = false;

        public void Error(int line, string message)
        {
            Report(line, "", message);
        }

        public void Error(Token token, string message)
        {
            if (token.type == TokenType.EOF)
            {
                Report(token.line, " at end", message);
            }
            else
            {
                Report(token.line, $" at '{token.lexeme}'", message);
            }
        }

        public void RuntimeError(RuntimeException err)
        {
            Console.WriteLine($"{err.Message} \n[line {err.token.line}]");
            hadRuntimeError = true;
        }

        private void Report(int line, string where, string message)
        {
            Console.WriteLine($"[line {line}] Error {where}: {message}");
            hadError = true;
        }
    }

    [TestClass]
    public class ScannerTest
    {
        [TestMethod]
        public void Test_Scanner()
        {
            IErrorListener listener = new MockListener();
            Scanner scanner = new Scanner("var a = 5;", listener);
            var tokens = scanner.ScanTokens();
            Assert.AreEqual(tokens[0].type, TokenType.VAR);
            Assert.AreEqual(tokens[1].type, TokenType.IDENTIFIER);
            Assert.AreEqual(tokens[2].type, TokenType.EQUAL);
            Assert.AreEqual(tokens[3].type, TokenType.NUMBER);
        }
    }
}
