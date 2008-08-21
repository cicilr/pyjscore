import jscore
import unittest

class TestBasic(unittest.TestCase):
    def testContextCreate(self):
        self.assert_(isinstance(jscore.Context(), jscore.Context))

    def testGetGlobalObject(self):
        self.assert_(jscore.Context().globalObject is not None)

    def textGlobalEval(self):
        self.assertEqual(jscore.Context().globalObject.eval('true'), True)

    def testPrimitiveTypes(self):
        g = jscore.Context().globalObject
        self.assertEqual(g.eval('1'), 1)
        self.assertEqual(type(g.eval('1')), float)
        self.assertEqual(g.eval("'1'"), '1')
        self.assertEqual(g.eval('true'), True)
        self.assertEqual(g.eval('false'), False)
        self.assertEqual(g.eval('null'), jscore.null)
        self.assertEqual(g.eval('undefined'), None)

        self.assertEqual(repr(g.eval('Infinity')), 'inf')
        self.assertEqual(repr(g.eval('NaN')), 'nan')

    def testUnicode(self):
        g = jscore.Context().globalObject
        self.assertEqual(g.eval(u"'\u263a'"), u'\u263a')
        self.assertEqual(g.eval("'\\u263a'"), u'\u263a')

class TestJSProxyObjects(unittest.TestCase):
    def setUp(self):
        self.c = jscore.Context()
        self.g = self.c.globalObject

    def testProperties(self):
        g = self.g
        g.eval('a = {b: 1}')
        self.assertEqual(g.a.b, 1)
        self.assertRaises(AttributeError, lambda: g.a.c)
        g.a.c = 2
        self.assertEqual(g.a.c, 2)
        self.assertEqual(g.eval('a.c'), 2)
        self.assert_(g.eval('a.c == 2'))
        del g.a.c
        self.assertRaises(AttributeError, lambda: g.a.c)
        self.assert_(g.eval('a.c == undefined'))

    def testFunctions(self):
        g = self.g
        self.assertEqual(g.parseFloat('1.5'), 1.5)
        self.assertEqual(g.String('1.5'), '1.5')

    def testMethods(self):
        g = self.g
        g.eval('foo = { bar: function() { return this.baz }, baz: 42};')
        self.assertEqual(g.foo.bar(), 42)

    def testIteration(self):
        g = self.g
        g.eval('foo = {a:1, b:2, c:3}; bar = ["a", "b", "c"]; baz = Object()')
        self.assertEqual(set(g.foo), set(['a', 'b', 'c']))
        self.assertEqual(set(g.bar), set(['0', '1', '2']))
        self.assert_(not set(g.baz))

    def testMapping(self):
        g = self.g
        g.eval('a=1')
        self.assert_('a' in g)
        self.assertEqual(g['a'], 1)
        g.a = 2
        self.assertEqual(g['a'], 2)
        g['a'] = 3
        self.assertEqual(g.a, 3)
        del g['a']
        self.assert_(not 'a' in g)
        self.assertEqual(g['a'], None)

class TestPyProxyObjects(unittest.TestCase):
    def setUp(self):
        self.c = jscore.Context()
        self.g = self.c.globalObject

    def testFunctions(self):
        g = self.g
        g.foo = lambda: 42
        g.add = lambda: a + b
        self.assertEqual(g.foo(), 42)
        self.assertEqual(g.eval('foo()'), 42)
        # XXX broken!
        # self.assertEqual(g.eval('add(40, 2)'), 42)
        # self.assertEqual(g.eval('add("4", "2")'), '42')
        
if __name__ == '__main__':
    unittest.main()