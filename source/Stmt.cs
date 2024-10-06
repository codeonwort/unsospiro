namespace UnSospiro
{
	internal abstract class Stmt
	{
		internal interface Visitor<R>
		{
			R VisitBlockStmt(Block stmt);
			R VisitExpressionStmt(Expression stmt);
			R VisitIfStmt(If stmt);
			R VisitPrintStmt(Print stmt);
			R VisitVarStmt(Var stmt);
			R VisitWhileStmt(While stmt);
		}
		internal class Block : Stmt
		{
			public Block(List<Stmt> statements)
			{
				this.statements = statements;
			}
			internal override R Accept<R>(Visitor<R> visitor)
			{
				return visitor.VisitBlockStmt(this);
			}
			internal List<Stmt> statements;
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
		internal class If : Stmt
		{
			public If(Expr condition, Stmt thenBranch, Stmt elseBranch)
			{
				this.condition = condition;
				this.thenBranch = thenBranch;
				this.elseBranch = elseBranch;
			}
			internal override R Accept<R>(Visitor<R> visitor)
			{
				return visitor.VisitIfStmt(this);
			}
			internal Expr condition;
			internal Stmt thenBranch;
			internal Stmt elseBranch;
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
		internal class Var : Stmt
		{
			public Var(Token name, Expr initializer)
			{
				this.name = name;
				this.initializer = initializer;
			}
			internal override R Accept<R>(Visitor<R> visitor)
			{
				return visitor.VisitVarStmt(this);
			}
			internal Token name;
			internal Expr initializer;
		}
		internal class While : Stmt
		{
			public While(Expr condition, Stmt body)
			{
				this.condition = condition;
				this.body = body;
			}
			internal override R Accept<R>(Visitor<R> visitor)
			{
				return visitor.VisitWhileStmt(this);
			}
			internal Expr condition;
			internal Stmt body;
		}

		internal abstract R Accept<R>(Visitor<R> visitor);
	}
}
