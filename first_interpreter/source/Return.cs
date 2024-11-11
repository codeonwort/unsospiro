namespace UnSospiro
{
    internal class Return : Exception
    {
        internal Object value;

        public Return(object value)
        {
            this.value = value;
        }
    }
}
