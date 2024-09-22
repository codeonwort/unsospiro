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
			internal Expr left;
			internal Token op;
			internal Expr right;
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
			internal Expr expression;
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
			internal Object value;
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
			internal Token op;
			internal Expr right;
		}

		internal abstract R Accept<R>(Visitor<R> visitor);
	}
}
