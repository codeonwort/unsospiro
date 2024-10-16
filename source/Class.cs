namespace UnSospiro
{
    internal class Class : Callable
    {
        private string name;

        public Class(string name)
        {
            this.name = name;
        }

        public override string ToString() => name;

        public Object Call(Interpreter interpreter, List<Object> arguments)
        {
            Instance instance = new Instance(this);
            return instance;
        }

        public int Arity() => 0; // TODO: Constructor does not take any arguments for now.

        // TODO: Implement fields and methods.
    }
}
