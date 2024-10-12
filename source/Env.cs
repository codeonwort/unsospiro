namespace UnSospiro
{
    internal class Env
    {
        Env enclosing;
        private Dictionary<string, Object> values = new();

        public Env()
        {
            this.enclosing = null;
        }

        public Env(Env enclosing)
        {
            this.enclosing = enclosing;
        }

        public void Define(string name, Object value)
        {
            values[name] = value;
        }

        public void Assign(Token name, Object value)
        {
            if (values.ContainsKey(name.lexeme))
            {
                values[name.lexeme] = value;
                return;
            }

            if (enclosing != null)
            {
                enclosing.Assign(name, value);
                return;
            }

            throw new RuntimeException(name, $"Undefined variable '{name.lexeme}'.");
        }

        public void AssignAt(int distance, Token name, Object value)
        {
            Ancestor(distance).values[name.lexeme] = value;
        }

        public Object Get(Token name)
        {
            if (values.TryGetValue(name.lexeme, out Object value))
            {
                return value;
            }

            if (enclosing != null) return enclosing.Get(name);

            throw new RuntimeException(name, $"Undefined variable '{name.lexeme}'.");
        }

        public Object GetAt(int distance, string name)
        {
            return Ancestor(distance).values[name];
        }

        private Env Ancestor(int distance)
        {
            Env env = this;
            for (int i = 0; i < distance; ++i)
            {
                env = env.enclosing;
            }
            return env;
        }
    }
}
