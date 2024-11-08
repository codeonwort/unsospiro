using System.Text;

namespace UnSospiro
{
    internal class AstPrinter : Expr.Visitor<string>, Stmt.Visitor<string>
    {
        internal string Print(Stmt stmt)
        {
            return stmt.Accept(this);
        }
        internal string Print(Expr expr)
        {
            return expr.Accept(this);
        }

        // ----------------------------------------------------------
        // Interface: Expr.Visitor

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

        public string VisitSuperExpr(Expr.Super expr)
        {
            return Parenthesize("super", expr);
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

        // ----------------------------------------------------------
        // Interface: Stmt.Visitor

        public string VisitBlockStmt(Stmt.Block blockStmt)
        {
            StringBuilder writer = new();
            writer.Append("{\n");
            foreach (var stmt in blockStmt.statements)
            {
                writer.Append($"\t{stmt.Accept(this)}\n");
            }
            writer.Append("}");
            return writer.ToString();
        }
        public string VisitClassStmt(Stmt.Class stmt)
        {
            // TODO: Correct impl.
            return Parenthesize($"class {stmt.name.lexeme}");
        }
        public string VisitExpressionStmt(Stmt.Expression stmt)
        {
            return stmt.expression.Accept(this);
        }
        public string VisitFunctionStmt(Stmt.Function stmt)
        {
            StringBuilder writer = new();
            writer.Append($"(function {stmt.name.lexeme} (");
            for (var i = 0; i < stmt.parameters.Count; ++i)
            {
                writer.Append(stmt.parameters[i].lexeme);
                if (i != stmt.parameters.Count - 1) writer.Append(' ');
            }
            writer.Append(")\n");
            writer.Append(Parenthesize(stmt.body, "\t"));
            writer.Append(')');
            return writer.ToString();
        }
        public string VisitIfStmt(Stmt.If stmt)
        {
            // TODO: Correct impl.
            return Parenthesize("if");
        }
        public string VisitPrintStmt(Stmt.Print stmt)
        {
            // TODO: Correct impl.
            return Parenthesize("print");
        }
        public string VisitReturnStmt(Stmt.Return stmt)
        {
            // TODO: Correct impl.
            return Parenthesize("return");
        }
        public string VisitVarStmt(Stmt.Var stmt)
        {
            // TODO: Correct impl.
            return Parenthesize($"var {stmt.name.lexeme}");
        }
        public string VisitWhileStmt(Stmt.While stmt)
        {
            // TODO: Correct impl.
            return Parenthesize("while");
        }

        // ----------------------------------------------------------
        // Utils

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
        private string Parenthesize(List<Stmt> statements, string indent = "")
        {
            StringBuilder writer = new();
            foreach (var stmt in statements)
            {
                writer.Append($"{indent}{stmt.Accept(this)}\n");
            }
            return writer.ToString();
        }
    }
}
