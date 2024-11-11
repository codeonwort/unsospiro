namespace UnSospiro
{
    internal class RuntimeException : Exception
    {
        public Token token { get; private set; }

        public RuntimeException(Token token, string message)
            : base(message)
        {
            this.token = token;
        }
    }
}
