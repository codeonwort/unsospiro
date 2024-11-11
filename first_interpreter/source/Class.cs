namespace UnSospiro
{
    internal class Class : Callable
    {
        private string name;
        private Class superclass;
        private Dictionary<string, Function> methods;

        public Class(string name, Class superclass, Dictionary<string, Function> methods)
        {
            this.name = name;
            this.superclass = superclass;
            this.methods = methods;
        }

        public override string ToString() => name;

        public Function FindMethod(string name)
        {
            if (methods.TryGetValue(name, out var method))
            {
                return method;
            }
            if (superclass != null)
            {
                return superclass.FindMethod(name);
            }
            return null;
        }

        public Object Call(Interpreter interpreter, List<Object> arguments)
        {
            Instance instance = new Instance(this);
            Function initializer = FindMethod("init");
            if (initializer != null)
            {
                initializer.Bind(instance).Call(interpreter, arguments);
            }
            return instance;
        }

        public int Arity()
        {
            Function initializer = FindMethod("init");
            if (initializer == null) return 0;
            return initializer.Arity();
        }
    }
}
