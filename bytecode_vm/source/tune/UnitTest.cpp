#include "pch.h"
#include "CppUnitTest.h"

#include "clavier/vm.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace UnitTest
{
	TEST_CLASS(UnitTest)
	{
	public:
		
		TEST_METHOD(InitAndFreeVM)
		{
			VM vm;
			initVM(&vm);
			freeVM(&vm);

			Assert::IsTrue(true);
		}
	};
}
