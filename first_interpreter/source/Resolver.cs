﻿namespace UnSospiro
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
            INITIALIZER,
            METHOD
        }

        private enum ClassType
        {
            NONE,
            CLASS,
            SUBCLASS
        }

        private IErrorListener errorListener;

        private Interpreter interpreter;
        private Stack<Dictionary<string, bool>> scopes = new();
        private FunctionType currentFunction = FunctionType.NONE;
        private ClassType currentClass = ClassType.NONE;

        public Resolver(Interpreter interpreter, IErrorListener errorListener)
        {
            this.interpreter = interpreter;
            this.errorListener = errorListener;
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
                    errorListener.Error(expr.name, "Can't read local variable in its own initializer.");
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

        public Void VisitSuperExpr(Expr.Super expr)
        {
            if (currentClass == ClassType.NONE)
            {
                errorListener.Error(expr.keyword, "Can't use 'super' outside of a class.");
            }
            else if (currentClass != ClassType.SUBCLASS)
            {
                errorListener.Error(expr.keyword, "Can't use 'super' in a class with no superclass.");
            }
            ResolveLocal(expr, expr.keyword);
            return Void.Instance;
        }

        public Void VisitThisExpr(Expr.This expr)
        {
            if (currentClass == ClassType.NONE)
            {
                errorListener.Error(expr.keyword, "Can't use 'this' outside of a class.");
                return Void.Instance;
            }
            ResolveLocal(expr, expr.keyword);
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
            ClassType enclosingClass = currentClass;
            currentClass = ClassType.CLASS;

            Declare(stmt.name);
            Define(stmt.name);

            if (stmt.superclass != null && stmt.name.lexeme.Equals(stmt.superclass.name.lexeme))
            {
                errorListener.Error(stmt.superclass.name, "A class can't inherit from itself.");
            }

            if (stmt.superclass != null)
            {
                currentClass = ClassType.SUBCLASS;
                Resolve(stmt.superclass);
            }

            if (stmt.superclass != null)
            {
                BeginScope();
                scopes.Peek().Add("super", true);
            }

            BeginScope();
            scopes.Peek().Add("this", true);

            foreach (Stmt.Function method in stmt.methods)
            {
                FunctionType declaration = FunctionType.METHOD;
                if (method.name.lexeme.Equals("init"))
                {
                    declaration = FunctionType.INITIALIZER;
                }
                ResolveFunction(method, declaration);
            }

            EndScope();

            if (stmt.superclass != null)
            {
                EndScope();
            }

            currentClass = enclosingClass;
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
                errorListener.Error(stmt.keyword, "Can't return from top-level code.");
            }

            if (stmt.value != null)
            {
                if (currentFunction == FunctionType.INITIALIZER)
                {
                    errorListener.Error(stmt.keyword, "Can't return a value from an initializer.");
                }
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
            // NOTE: The book's source code is written in Java. In Java, Stack.get(0) returns the first element we put in the stack.
            // In C#, Stack.ElementAt(0) returns the last element we put. They are in reverse order :(
            for (int i = 0; i < scopes.Count; ++i)
            {
                if (scopes.ElementAt(i).ContainsKey(name.lexeme))
                {
                    interpreter.Resolve(expr, i);
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
                errorListener.Error(name, "Already a variable with this name exists in this scope.");
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
