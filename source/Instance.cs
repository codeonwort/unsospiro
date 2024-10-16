namespace UnSospiro
{
    internal class Instance
    {
        private Class klass;

        public Instance(Class klass)
        {
            this.klass = klass;
        }

        public override string ToString() => $"{klass.ToString()} instance";
    }
}
