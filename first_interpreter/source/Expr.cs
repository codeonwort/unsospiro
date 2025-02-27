namespace UnSospiro
{
	internal abstract class Expr
	{
		internal interface Visitor<R>
		{
			R VisitAssignExpr(Assign expr);
			R VisitBinaryExpr(Binary expr);
			R VisitCallExpr(Call expr);
			R VisitGetExpr(Get expr);
			R VisitGroupingExpr(Grouping expr);
			R VisitLiteralExpr(Literal expr);
			R VisitLogicalExpr(Logical expr);
			R VisitSetExpr(Set expr);
			R VisitSuperExpr(Super expr);
			R VisitThisExpr(This expr);
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
		internal class Call : Expr
		{
			public Call(Expr callee, Token paren, List<Expr> arguments)
			{
				this.callee = callee;
				this.paren = paren;
				this.arguments = arguments;
			}
			internal override R Accept<R>(Visitor<R> visitor)
			{
				return visitor.VisitCallExpr(this);
			}
			internal Expr callee;
			internal Token paren;
			internal List<Expr> arguments;
		}
		internal class Get : Expr
		{
			public Get(Expr obj, Token name)
			{
				this.obj = obj;
				this.name = name;
			}
			internal override R Accept<R>(Visitor<R> visitor)
			{
				return visitor.VisitGetExpr(this);
			}
			internal Expr obj;
			internal Token name;
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
		internal class Logical : Expr
		{
			public Logical(Expr left, Token op, Expr right)
			{
				this.left = left;
				this.op = op;
				this.right = right;
			}
			internal override R Accept<R>(Visitor<R> visitor)
			{
				return visitor.VisitLogicalExpr(this);
			}
			internal Expr left;
			internal Token op;
			internal Expr right;
		}
		internal class Set : Expr
		{
			public Set(Expr obj, Token name, Expr value)
			{
				this.obj = obj;
				this.name = name;
				this.value = value;
			}
			internal override R Accept<R>(Visitor<R> visitor)
			{
				return visitor.VisitSetExpr(this);
			}
			internal Expr obj;
			internal Token name;
			internal Expr value;
		}
		internal class Super : Expr
		{
			public Super(Token keyword, Token method)
			{
				this.keyword = keyword;
				this.method = method;
			}
			internal override R Accept<R>(Visitor<R> visitor)
			{
				return visitor.VisitSuperExpr(this);
			}
			internal Token keyword;
			internal Token method;
		}
		internal class This : Expr
		{
			public This(Token keyword)
			{
				this.keyword = keyword;
			}
			internal override R Accept<R>(Visitor<R> visitor)
			{
				return visitor.VisitThisExpr(this);
			}
			internal Token keyword;
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
