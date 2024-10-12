namespace UnSospiro
{
    internal class Interpreter : Expr.Visitor<Object>, Stmt.Visitor<Void>
    {
        private Env globals;
        private Env environment;
        private Dictionary<Expr, int> locals = new();

        internal Env Globals => globals;

        class ClockFunction : Callable
        {
            public int Arity() { return 0; }

            public Object Call(Interpreter interpreter, List<Object> arguments)
            {
                return (double)DateTimeOffset.UtcNow.ToUnixTimeMilliseconds();
            }

            public override string ToString() => "<native fn>";
        }

        public Interpreter()
        {
            globals = new();
            globals.Define("clock", new ClockFunction());

            environment = globals;
        }

        public void Interpret(List<Stmt> statements)
        {
            try
            {
                foreach (Stmt stmt in statements)
                {
                    Execute(stmt);
                }
            }
            catch (RuntimeException err)
            {
                MainProgram.RuntimeError(err);
            }
        }

        public void Resolve(Expr expr, int depth)
        {
            locals[expr] = depth;
        }

        // ----------------------------------------------------------
        // Interface: Expr.Visitor

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
                    CheckNumberOperand(expr.op, right);
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
                    CheckNumberOperands(expr.op, left, right);
                    return (double)left > (double)right;
                case TokenType.GREATER_EQUAL:
                    CheckNumberOperands(expr.op, left, right);
                    return (double)left >= (double)right;
                case TokenType.LESS:
                    CheckNumberOperands(expr.op, left, right);
                    return (double)left < (double)right;
                case TokenType.LESS_EQUAL:
                    CheckNumberOperands(expr.op, left, right);
                    return (double)left <= (double)right;
                case TokenType.BANG_EQUAL:
                    return !IsEqual(left, right);
                case TokenType.EQUAL_EQUAL:
                    return IsEqual(left, right);
                case TokenType.MINUS:
                    CheckNumberOperands(expr.op, left, right);
                    return (double)left - (double)right;
                case TokenType.PLUS:
                    if (left is double && right is double) return (double)left + (double)right;
                    if (left is string && right is string) return (string)left + (string)right;
                    throw new RuntimeException(expr.op, "Operands must be two numbers or two strings.");
                    break;
                case TokenType.SLASH:
                    CheckNumberOperands(expr.op, left, right);
                    if ((double)right == 0.0)
                    {
                        throw new RuntimeException(expr.op, "Divide by zero.");
                    }
                    return (double)left / (double)right;
                case TokenType.STAR:
                    CheckNumberOperands(expr.op, left, right);
                    return (double)left * (double)right;
            }
            return null;
        }

        public Object VisitVariableExpr(Expr.Variable expr)
        {
            return LookUpVariable(expr.name, expr);
        }

        public Object VisitAssignExpr(Expr.Assign expr)
        {
            Object value = Evaluate(expr.value);

            if (locals.TryGetValue(expr, out int distance))
            {
                environment.AssignAt(distance, expr.name, value);
            }
            else
            {
                globals.Assign(expr.name, value);
            }

            return value;
        }

        public Object VisitLogicalExpr(Expr.Logical expr)
        {
            Object left = Evaluate(expr.left);

            if (expr.op.type == TokenType.OR)
            {
                if (IsTruthy(left)) return left;
            }
            else
            {
                if (!IsTruthy(left)) return left;
            }

            return Evaluate(expr.right);
        }

        public Object VisitCallExpr(Expr.Call expr)
        {
            Object callee = Evaluate(expr.callee);

            List<Object> arguments = new();
            foreach (Expr argument in expr.arguments)
            {
                arguments.Add(Evaluate(argument));
            }

            if (!(callee is Callable))
            {
                throw new RuntimeException(expr.paren, "Can only call functions and classes.");
            }

            Callable function = (Callable)callee;

            if (arguments.Count != function.Arity())
            {
                throw new RuntimeException(expr.paren, $"Expected {function.Arity()} arguments but got {arguments.Count}.");
            }

            return function.Call(this, arguments);
        }

        // ----------------------------------------------------------
        // Interface: Stmt.Visitor

        public Void VisitIfStmt(Stmt.If stmt)
        {
            if (IsTruthy(Evaluate(stmt.condition)))
            {
                Execute(stmt.thenBranch);
            }
            else if (stmt.elseBranch != null)
            {
                Execute(stmt.elseBranch);
            }
            return Void.Instance;
        }

        public Void VisitBlockStmt(Stmt.Block stmt)
        {
            ExecuteBlock(stmt.statements, new Env(environment));
            return Void.Instance;
        }

        public Void VisitExpressionStmt(Stmt.Expression stmt)
        {
            Evaluate(stmt.expression);
            return Void.Instance;
        }

        public Void VisitPrintStmt(Stmt.Print stmt)
        {
            Object value = Evaluate(stmt.expression);
            Console.WriteLine(Stringify(value));
            return Void.Instance;
        }

        public Void VisitReturnStmt(Stmt.Return stmt)
        {
            Object value = null;
            if (stmt.value != null)
            {
                value = Evaluate(stmt.value);
            }
            // Use exception to implement return...?
            throw new Return(value);
        }

        public Void VisitVarStmt(Stmt.Var stmt)
        {
            Object value = null; // Default value is nil.
            if (stmt.initializer != null)
            {
                value = Evaluate(stmt.initializer);
            }

            environment.Define(stmt.name.lexeme, value);
            return Void.Instance;
        }

        public Void VisitWhileStmt(Stmt.While stmt)
        {
            while (IsTruthy(Evaluate(stmt.condition))) 
            {
                Execute(stmt.body);
            }
            return Void.Instance;
        }

        public Void VisitFunctionStmt(Stmt.Function stmt)
        {
            Function function = new Function(stmt, environment);
            environment.Define(stmt.name.lexeme, function);
            return null;
        }

        // ----------------------------------------------------------
        // Utils

        internal void ExecuteBlock(List<Stmt> statements, Env environment)
        {
            Env previous = this.environment;
            try
            {
                this.environment = environment;
                foreach (Stmt statement in statements)
                {
                    Execute(statement);
                }
            }
            finally
            {
                this.environment = previous;
            }
        }

        private void Execute(Stmt stmt)
        {
            stmt.Accept(this);
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

        private void CheckNumberOperand(Token op, Object operand)
        {
            if (operand is double) return;
            throw new RuntimeException(op, "Operand must be a number.");
        }

        private void CheckNumberOperands(Token op, Object left, Object right)
        {
            if (left is double && right is double) return;
            throw new RuntimeException(op, "Operands must be numbers.");
        }

        private string Stringify(Object obj)
        {
            if (obj == null) return "nil";
            if (obj is double)
            {
                string text = obj.ToString();
                if (text.EndsWith(".0"))
                {
                    text = text.Substring(0, text.Length - 2);
                }
                return text;
            }
            return obj.ToString();
        }

        private Object LookUpVariable(Token name, Expr expr)
        {
            if (locals.TryGetValue(expr, out int distance))
            {
                return environment.GetAt(distance, name.lexeme);
            }
            return globals.Get(name);
        }
    }
}
