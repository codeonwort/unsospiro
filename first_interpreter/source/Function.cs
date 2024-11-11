namespace UnSospiro
{
    internal class Function : Callable
    {
        private Stmt.Function declaration;
        private Env closure;
        private bool isInitializer;

        public Function(Stmt.Function declaration, Env closure, bool isInitializer)
        {
            this.declaration = declaration;
            this.closure = closure;
            this.isInitializer = isInitializer;
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
                if (isInitializer) return closure.GetAt(0, "this");
                return returnValue.value;
            }
            if (isInitializer) return closure.GetAt(0, "this");
            return null;
        }

        public int Arity() => declaration.parameters.Count;

        public Function Bind(Instance instance)
        {
            Env environment = new(closure);
            environment.Define("this", instance);
            return new Function(declaration, environment, isInitializer);
        }

        public override string ToString() => $"<fn {declaration.name.lexeme} >";
    }
}
