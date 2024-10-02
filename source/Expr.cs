namespace UnSospiro
{
	internal abstract class Expr
	{
		internal interface Visitor<R>
		{
			R VisitAssignExpr(Assign expr);
			R VisitBinaryExpr(Binary expr);
			R VisitGroupingExpr(Grouping expr);
			R VisitLiteralExpr(Literal expr);
			R VisitUnaryExpr(Unary expr);
			R VisitVariableExpr(Variable expr);
		}
		internal class Assign : Expr
		{
			public Assign(Token name, Expr value)
			{
				this.name = name;
				this.value = value;
			}
			internal override R Accept<R>(Visitor<R> visitor)
			{
				return visitor.VisitAssignExpr(this);
			}
			internal Token name;
			internal Expr value;
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
		internal class Variable : Expr
		{
			public Variable(Token name)
			{
				this.name = name;
			}
			internal override R Accept<R>(Visitor<R> visitor)
			{
				return visitor.VisitVariableExpr(this);
			}
			internal Token name;
		}

		internal abstract R Accept<R>(Visitor<R> visitor);
	}
}
