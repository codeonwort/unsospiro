using System.Text;

namespace UnSospiro
{
    internal class AstPrinter : Expr.Visitor<string>
    {
        internal string Print(Expr expr)
        {
            return expr.Accept(this);
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
