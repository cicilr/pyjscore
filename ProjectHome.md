pyjscore embeds a JavaScript environment into python, based on Apple's JavaScriptCore (from WebKit).

## Features ##
  * Bidirectional object bridging (incomplete)
  * Call JavaScript functions from Python, and vice versa (incomplete)
  * Powered by SquirrelFish, the VM that makes JavaScriptCore the fastest JavaScript engine
  * Fully Unicode-aware

## Future ##
  * Allow Python objects to subclass JavaScript ones?
  * Provide more Pythonic class for Array objects?

## Sample ##
```
>>> import jscore
>>> c = jscore.Context()
>>> Math = c.globalObject.Math
>>> Math.sin(Math.PI/2) + 3
4.0
>>> def print_(*args): 
...     for x in args[:-1]: print x,
...     print args[-1]
... 
>>> c.globalObject['print'] = print_
>>> c.eval('''
... function hello(name) {
...   print('Hello,', name + '!');
... }
... ''')
>>> c.globalObject.hello('world')
Hello, world!
```