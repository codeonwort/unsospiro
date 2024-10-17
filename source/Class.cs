namespace UnSospiro
{
    internal class Class : Callable
    {
        private string name;
        private Dictionary<string, Function> methods;

        public Class(string name, Dictionary<string, Function> methods)
        {
            this.name = name;
            this.methods = methods;
        }

        public override string ToString() => name;

        public Function FindMethod(string name)
        {
            return methods.GetValueOrDefault(name, null);
        }

        public Object Call(Interpreter interpreter, List<Object> arguments)
        {
            Instance instance = new Instance(this);
            return instance;
        }

        public int Arity() => 0; // TODO: Constructor does not take any arguments for now.
    }
}
