namespace UnSospiro
{
    internal interface Callable
    {
        int Arity(); // The number of arguments

        Object Call(Interpreter interpreter, List<Object> arguments);
    }
}
