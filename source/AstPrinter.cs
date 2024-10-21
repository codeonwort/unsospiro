using System.Text;

namespace UnSospiro
{
    internal class AstPrinter : Expr.Visitor<string>
    {
        internal string Print(Expr expr)
        {
            return expr.Accept(this);
        }

        public string VisitAssignExpr(Expr.Assign expr)
        {
            return Parenthesize($"assign {expr.name.lexeme}", expr.value);
        }

        public string VisitBinaryExpr(Expr.Binary expr)
        {
            return Parenthesize(expr.op.lexeme, expr.left, expr.right);
        }

        public string VisitGroupingExpr(Expr.Grouping expr)
        {
            return Parenthesize("group", expr.expression);
        }

        public string VisitLiteralExpr(Expr.Literal expr)
        {
            if (expr.value == null) return "nil";
            return expr.value.ToString();
        }

        public string VisitUnaryExpr(Expr.Unary expr)
        {
            return Parenthesize(expr.op.lexeme, expr.right);
        }

        public string VisitVariableExpr(Expr.Variable expr)
        {
            return $"var {expr.name.lexeme}";
        }

        public string VisitLogicalExpr(Expr.Logical expr)
        {
            if (expr.op.type == TokenType.OR)
            {
                return Parenthesize("or", expr.left, expr.right);
            }
            return Parenthesize("and", expr.left, expr.right);
        }

        public string VisitSetExpr(Expr.Set expr)
        {
            return Parenthesize("set {expr.name}", expr.obj, expr.value);
        }

        public string VisitCallExpr(Expr.Call expr)
        {
            Expr[] calleeAndArgs = new Expr[expr.arguments.Count + 1];
            calleeAndArgs[0] = expr.callee;
            for (int i = 0; i < expr.arguments.Count; ++i)
            {
                calleeAndArgs[i + 1] = expr.arguments[i];
            }
            return Parenthesize("call", calleeAndArgs);
        }

        public string VisitGetExpr(Expr.Get expr)
        {
            return Parenthesize($"get {expr.name}", expr.obj);
        }

        public string VisitThisExpr(Expr.This expr)
        {
            return Parenthesize("this", expr);
        }

        private string Parenthesize(string name, params Expr[] exprs)
        {
            StringBuilder writer = new();
            writer.Append($"({name}");
            foreach (Expr expr in exprs)
            {
                writer.Append(' ');
                writer.Append(expr.Accept(this));
            }
            writer.Append(')');

            return writer.ToString();
        }
    }
}
