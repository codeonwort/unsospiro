namespace UnSospiro
{
    internal class Instance
    {
        private Class klass;
        private Dictionary<string, Object> fields = new();

        public Instance(Class klass)
        {
            this.klass = klass;
        }

        public override string ToString() => $"{klass.ToString()} instance";

        public Object Get(Token name)
        {
            if (fields.TryGetValue(name.lexeme, out Object value))
            {
                return value;
            }
            throw new RuntimeException(name, $"Undefined property '{name.lexeme}'.");
        }

        public void Set(Token name, Object value)
        {
            fields[name.lexeme] = value;
        }
    }
}
