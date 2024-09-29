namespace UnSospiro
{
	internal abstract class Stmt
	{
		internal interface Visitor<R>
		{
			R VisitExpressionStmt(Expression stmt);
			R VisitPrintStmt(Print stmt);
		}
		internal class Expression : Stmt
		{
			public Expression(Expr expression)
			{
				this.expression = expression;
			}
			internal override R Accept<R>(Visitor<R> visitor)
			{
				return visitor.VisitExpressionStmt(this);
			}
			internal Expr expression;
		}
		internal class Print : Stmt
		{
			public Print(Expr expression)
			{
				this.expression = expression;
			}
			internal override R Accept<R>(Visitor<R> visitor)
			{
				return visitor.VisitPrintStmt(this);
			}
			internal Expr expression;
		}

		internal abstract R Accept<R>(Visitor<R> visitor);
	}
}
