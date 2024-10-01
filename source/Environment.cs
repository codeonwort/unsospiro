using System.Collections;

namespace UnSospiro
{
    internal class GlobalEnvironment
    {
        private Dictionary<string, Object> values = new();

        public void Define(string name, Object value)
        {
            values.Add(name, value);
        }

        public Object Get(Token name)
        {
            if (values.TryGetValue(name.lexeme, out Object value))
            {
                return value;
            }
            throw new RuntimeException(name, $"Undefined variable '{name.lexeme}'.");
        }
    }
}
