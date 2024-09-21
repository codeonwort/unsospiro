namespace UnSospiro
{
	internal abstract class Expr
	{
		internal interface Visitor<R>
		{
			R VisitBinaryExpr(Binary expr);
			R VisitGroupingExpr(Grouping expr);
			R VisitLiteralExpr(Literal expr);
			R VisitUnaryExpr(Unary expr);
		}
		internal class Binary : Expr
		{
			public Binary(Expr left, Token op, Expr right)
			{
				this.left = left;
				this.op = op;
				this.right = right;
			}
			internal override R Accept<R>(Visitor<R> visitor)
			{
				return visitor.VisitBinaryExpr(this);
			}
			Expr left;
			Token op;
			Expr right;
		}
		internal class Grouping : Expr
		{
			public Grouping(Expr expression)
			{
				this.expression = expression;
			}
			internal override R Accept<R>(Visitor<R> visitor)
			{
				return visitor.VisitGroupingExpr(this);
			}
			Expr expression;
		}
		internal class Literal : Expr
		{
			public Literal(Object value)
			{
				this.value = value;
			}
			internal override R Accept<R>(Visitor<R> visitor)
			{
				return visitor.VisitLiteralExpr(this);
			}
			Object value;
		}
		internal class Unary : Expr
		{
			public Unary(Token op, Expr right)
			{
				this.op = op;
				this.right = right;
			}
			internal override R Accept<R>(Visitor<R> visitor)
			{
				return visitor.VisitUnaryExpr(this);
			}
			Token op;
			Expr right;
		}

		internal abstract R Accept<R>(Visitor<R> visitor);
	}
}
