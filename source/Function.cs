namespace UnSospiro
{
    internal class Function : Callable
    {
        private Stmt.Function declaration;
        private Env closure;

        public Function(Stmt.Function declaration, Env closure)
        {
            this.declaration = declaration;
            this.closure = closure;
        }

        public Object Call(Interpreter interpreter, List<Object> arguments)
        {
            Env env = new Env(closure);
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

        public Function Bind(Instance instance)
        {
            Env environment = new(closure);
            environment.Define("this", instance);
            return new Function(declaration, environment);
        }

        public override string ToString() => $"<fn {declaration.name.lexeme} >";
    }
}
