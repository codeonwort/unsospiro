namespace UnSospiro
{
    /// <summary>
    /// Resolver resolves variables, after parser constructs a syntax tree, but before interpreter executes the tree.
    /// Resolver has no side effects and no control flows.
    /// </summary>
    internal class Resolver : Expr.Visitor<Void>, Stmt.Visitor<Void>
    {
        private enum FunctionType
        {
            NONE,
            FUNCTION,
            METHOD
        }

        private Interpreter interpreter;
        private Stack<Dictionary<string, bool>> scopes = new();
        private FunctionType currentFunction = FunctionType.NONE;

        public Resolver(Interpreter interpreter)
        {
            this.interpreter = interpreter;
        }

        public void Resolve(List<Stmt> statements)
        {
            foreach (Stmt stmt in statements)
            {
                Resolve(stmt);
            }
        }

        // ----------------------------------------------------------
        // Interface: Expr.Visitor

        public Void VisitVariableExpr(Expr.Variable expr)
        {
            if (scopes.Count > 0 && scopes.Peek().TryGetValue(expr.name.lexeme, out bool value))
            {
                if (value == false)
                {
                    MainProgram.Error(expr.name, "Can't read local variable in its own initializer.");
                }
            }
            ResolveLocal(expr, expr.name);
            return Void.Instance;
        }

        public Void VisitAssignExpr(Expr.Assign expr)
        {
            Resolve(expr.value);
            ResolveLocal(expr, expr.name);
            return Void.Instance;
        }

        public Void VisitCallExpr(Expr.Call expr)
        {
            Resolve(expr.callee);
            foreach (Expr argument in expr.arguments)
            {
                Resolve(argument);
            }
            return Void.Instance;
        }

        public Void VisitGetExpr(Expr.Get expr)
        {
            Resolve(expr.obj);
            return Void.Instance;
        }

        public Void VisitGroupingExpr(Expr.Grouping expr)
        {
            Resolve(expr.expression);
            return Void.Instance;
        }

        public Void VisitLiteralExpr(Expr.Literal expr)
        {
            return Void.Instance;
        }

        public Void VisitLogicalExpr(Expr.Logical expr)
        {
            Resolve(expr.left);
            Resolve(expr.right);
            return Void.Instance;
        }

        public Void VisitSetExpr(Expr.Set expr)
        {
            Resolve(expr.value);
            Resolve(expr.obj);
            return Void.Instance;
        }

        public Void VisitBinaryExpr(Expr.Binary expr)
        {
            Resolve(expr.left);
            Resolve(expr.right);
            return Void.Instance;
        }

        public Void VisitUnaryExpr(Expr.Unary expr)
        {
            Resolve(expr.right);
            return Void.Instance;
        }

        // ----------------------------------------------------------
        // Interface: Stmt.Visitor

        public Void VisitBlockStmt(Stmt.Block stmt)
        {
            BeginScope();
            Resolve(stmt.statements);
            EndScope();
            return Void.Instance;
        }

        public Void VisitClassStmt(Stmt.Class stmt)
        {
            Declare(stmt.name);
            Define(stmt.name);

            foreach (Stmt.Function method in stmt.methods)
            {
                FunctionType declaration = FunctionType.METHOD;
                ResolveFunction(method, declaration);
            }

            return Void.Instance;
        }

        public Void VisitVarStmt(Stmt.Var stmt)
        {
            Declare(stmt.name);
            if (stmt.initializer != null)
            {
                Resolve(stmt.initializer);
            }
            Define(stmt.name);
            return Void.Instance;
        }

        public Void VisitFunctionStmt(Stmt.Function stmt)
        {
            Declare(stmt.name);
            Define(stmt.name);

            ResolveFunction(stmt, FunctionType.FUNCTION);
            return Void.Instance;
        }

        public Void VisitExpressionStmt(Stmt.Expression stmt)
        {
            Resolve(stmt.expression);
            return Void.Instance;
        }

        public Void VisitIfStmt(Stmt.If stmt)
        {
            Resolve(stmt.condition);
            Resolve(stmt.thenBranch);
            if (stmt.elseBranch != null) Resolve(stmt.elseBranch);
            return Void.Instance;
        }

        public Void VisitPrintStmt(Stmt.Print stmt)
        {
            Resolve(stmt.expression);
            return Void.Instance;
        }

        public Void VisitReturnStmt(Stmt.Return stmt)
        {
            if (currentFunction == FunctionType.NONE)
            {
                MainProgram.Error(stmt.keyword, "Can't return from top-level code.");
            }

            if (stmt.value != null)
            {
                Resolve(stmt.value);
            }
            return Void.Instance;
        }

        public Void VisitWhileStmt(Stmt.While stmt)
        {
            Resolve(stmt.condition);
            Resolve(stmt.body);
            return Void.Instance;
        }

        // ----------------------------------------------------------
        // Utils

        private void Resolve(Stmt stmt)
        {
            stmt.Accept(this);
        }

        private void ResolveFunction(Stmt.Function function, FunctionType type)
        {
            FunctionType enclosingFunction = currentFunction;
            currentFunction = type;

            BeginScope();
            foreach (Token param in function.parameters)
            {
                Declare(param);
                Define(param);
            }
            Resolve(function.body);
            EndScope();

            currentFunction = enclosingFunction;
        }

        private void Resolve(Expr expr)
        {
            expr.Accept(this);
        }

        private void ResolveLocal(Expr expr, Token name)
        {
            for (int i = scopes.Count - 1; i >= 0; --i)
            {
                if (scopes.ElementAt(i).ContainsKey(name.lexeme))
                {
                    interpreter.Resolve(expr, scopes.Count - 1 - i);
                    return;
                }
            }
        }

        private void BeginScope()
        {
            scopes.Push(new Dictionary<string, bool>());
        }

        private void EndScope()
        {
            scopes.Pop();
        }

        private void Declare(Token name)
        {
            if (scopes.Count == 0) return;
            var scope = scopes.Peek();
            if (scope.ContainsKey(name.lexeme))
            {
                MainProgram.Error(name, "Already a variable with this name exists in this scope.");
            }

            scope[name.lexeme] = false;
        }

        private void Define(Token name)
        {
            if (scopes.Count == 0) return;
            scopes.Peek()[name.lexeme] = true;
        }
    }
}
