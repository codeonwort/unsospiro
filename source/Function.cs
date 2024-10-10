namespace UnSospiro
{
    internal class Function : Callable
    {
        private Stmt.Function declaration;

        public Function(Stmt.Function declaration)
        {
            this.declaration = declaration;
        }

        public Object Call(Interpreter interpreter, List<Object> arguments)
        {
            Env env = new Env(interpreter.Globals);
            for (int i = 0; i < declaration.parameters.Count; ++i)
            {
                env.Define(declaration.parameters[i].lexeme, arguments[i]);
            }
            try
            {
                interpreter.ExecuteBlock(declaration.body, env);
            }
            catch (Return returnValue)
            {
                return returnValue.value;
            }
            return null;
        }

        public int Arity() => declaration.parameters.Count;

        public override string ToString() => $"<fn {declaration.name.lexeme} >";
    }
}
