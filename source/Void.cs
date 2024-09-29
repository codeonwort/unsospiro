namespace UnSospiro
{
    // Silly boxed void
    internal class Void
    {
        private static readonly Void instance = new Void();

        public static Void Instance => instance;

        private Void() { }
    }
}
