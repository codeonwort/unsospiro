namespace UnSospiro
{
    // TODO: Handle runtime errors
    internal class Interpreter : Expr.Visitor<Object>
    {
        public Object VisitLiteralExpr(Expr.Literal expr)
        {
            return expr.value;
        }

        public Object VisitGroupingExpr(Expr.Grouping expr)
        {
            return Evaluate(expr.expression);
        }

        public Object VisitUnaryExpr(Expr.Unary expr)
        {
            Object right = Evaluate(expr.right);
            switch (expr.op.type)
            {
                case TokenType.BANG:
                    return !IsTruthy(right);
                case TokenType.MINUS:
                    return -(double)right;
            }
            return null;
        }

        public Object VisitBinaryExpr(Expr.Binary expr)
        {
            Object left = Evaluate(expr.left);
            Object right = Evaluate(expr.right);
            switch (expr.op.type)
            {
                case TokenType.GREATER:
                    return (double)left > (double)right;
                case TokenType.GREATER_EQUAL:
                    return (double)left >= (double)right;
                case TokenType.LESS:
                    return (double)left < (double)right;
                case TokenType.LESS_EQUAL:
                    return (double)left <= (double)right;
                case TokenType.BANG_EQUAL:
                    return !IsEqual(left, right);
                case TokenType.EQUAL_EQUAL:
                    return IsEqual(left, right);
                case TokenType.MINUS:
                    return (double)left - (double)right;
                case TokenType.PLUS:
                    if (left is double && right is double) return (double)left + (double)right;
                    if (left is string && right is string) return (string)left + (string)right;
                    break;
                case TokenType.SLASH:
                    return (double)left / (double)right;
                case TokenType.STAR:
                    return (double)left * (double)right;
            }
            return null;
        }

        private Object Evaluate(Expr expr)
        {
            return expr.Accept(this);
        }

        private bool IsTruthy(Object obj)
        {
            if (obj == null) return false;
            if (obj is bool) return (bool)obj;
            return true;
        }

        private bool IsEqual(Object a, Object b)
        {
            if (a == null && b == null) return true;
            if (a == null) return false;
            return a.Equals(b);
        }
    }
}
