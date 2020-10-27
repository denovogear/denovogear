(function e(t,n,r){function s(o,u){if(!n[o]){if(!t[o]){var a=typeof require=="function"&&require;if(!u&&a)return a(o,!0);if(i)return i(o,!0);var f=new Error("Cannot find module '"+o+"'");throw f.code="MODULE_NOT_FOUND",f}var l=n[o]={exports:{}};t[o][0].call(l.exports,function(e){var n=t[o][1][e];return s(n?n:e)},l,l.exports,e,t,n,r)}return n[o].exports}var i=typeof require=="function"&&require;for(var o=0;o<r.length;o++)s(r[o]);return s})({1:[function(require,module,exports){
/**
 * Extended Newick format parser in JavaScript.
 *
 * Copyright (c) Miguel Pignatelli 2014 based on Jason Davies  
 *  
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *  
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *  
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * Example tree (from http://en.wikipedia.org/wiki/Newick_format):
 *
 * +--0.1--A
 * F-----0.2-----B            +-------0.3----C
 * +------------------0.5-----E
 *                            +---------0.4------D
 *
 * Newick format:
 * (A:0.1,B:0.2,(C:0.3,D:0.4)E:0.5)F;
 *
 * Converted to JSON:
 * {
 *   name: "F",
 *   children: [
 *     {name: "A", branch_length: 0.1},
 *     {name: "B", branch_length: 0.2},
 *     {
 *       name: "E",
 *       length: 0.5,
 *       children: [
 *         {name: "C", branch_length: 0.3},
 *         {name: "D", branch_length: 0.4}
 *       ]
 *     }
 *   ]
 * }
 *
 * Converted to JSON, but with no names or lengths:
 * {
 *   children: [
 *     {}, {}, {
 *       children: [{}, {}]
 *     }
 *   ]
 * }
 */

module.exports = parse_nhx = function(s) {
	var ancestors = [];
	var tree = {};
	// var tokens = s.split(/\s*(;|\(|\)|,|:)\s*/);
	//[&&NHX:D=N:G=ENSG00000139618:T=9606]
	var tokens = s.split( /\s*(;|\(|\)|\[|\]|,|:|=)\s*/ );
	for (var i=0; i<tokens.length; i++) {
		var token = tokens[i];
		switch (token) {
			case '(': // new children
				var subtree = {};
				tree.children = [subtree];
				ancestors.push(tree);
				tree = subtree;
				break;
			case ',': // another branch
				var subtree = {};
				ancestors[ancestors.length-1].children.push(subtree);
				tree = subtree;
				break;
			case ')': // optional name next
				tree = ancestors.pop();
				break;
			case ':': // optional length next
				break;
			default:
				var x = tokens[i-1];
				// var x2 = tokens[i-2];
				if (x == ')' || x == '(' || x == ',') {
					tree.name = token;
				} 
				else if (x == ':') {
					var test_type = typeof token;
					if(!isNaN(token)){
						tree.branch_length = parseFloat(token);
					}
					// tree.length = parseFloat(token);
				}
				else if (x == '='){
					var x2 = tokens[i-2];
					switch(x2){
						case 'D':
							tree.duplication = token; 
							break; 
						case 'G':
							tree.gene_id = token;
							break;
						case 'T':
							tree.taxon_id = token;
							break;

					}
				}
				else {
					var test;

				}
		}
	}
	return tree;
};


},{}],2:[function(require,module,exports){
module.exports = require('./newick');
module.exports.parse_nhx = require('./extended_newick');

},{"./extended_newick":1,"./newick":3}],3:[function(require,module,exports){
/**
 * Newick format parser in JavaScript.
 *
 * Copyright (c) edited by Miguel Pignatelli 2014, based on Jason Davies 2010.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * Example tree (from http://en.wikipedia.org/wiki/Newick_format):
 *
 * +--0.1--A
 * F-----0.2-----B            +-------0.3----C
 * +------------------0.5-----E
 *                            +---------0.4------D
 *
 * Newick format:
 * (A:0.1,B:0.2,(C:0.3,D:0.4)E:0.5)F;
 *
 * Converted to JSON:
 * {
 *   name: "F",
 *   children: [
 *     {name: "A", branch_length: 0.1},
 *     {name: "B", branch_length: 0.2},
 *     {
 *       name: "E",
 *       length: 0.5,
 *       children: [
 *         {name: "C", branch_length: 0.3},
 *         {name: "D", branch_length: 0.4}
 *       ]
 *     }
 *   ]
 * }
 *
 * Converted to JSON, but with no names or lengths:
 * {
 *   children: [
 *     {}, {}, {
 *       children: [{}, {}]
 *     }
 *   ]
 * }
 */



module.exports.parse_newick = function (s) {
	var ancestors = [];
	var tree = {};
	var tokens = s.split(/\s*(;|\(|\)|,|:)\s*/);
	for (var i=0; i<tokens.length; i++) {
		var token = tokens[i];
		switch (token) {
			case '(': // new children
				var subtree = {};
				tree.children = [subtree];
				ancestors.push(tree);
				tree = subtree;
				break;
			case ',': // another branch
				var subtree = {};
				ancestors[ancestors.length-1].children.push(subtree);
				tree = subtree;
				break;
			case ')': // optional name next
				tree = ancestors.pop();
				break;
			case ':': // optional length next
				break;
			default:
				var x = tokens[i-1];
				if (x == ')' || x == '(' || x == ',') {
					tree.name = token;
				} else if (x == ':') {
					tree.branch_length = parseFloat(token);
				}
		}
	}
	return tree;
};

module.exports.parse_json = function (json) {
	function nested(nest){
		var subtree = "";

		if(nest.hasOwnProperty('children')){
			var children = [];
			nest.children.forEach(function(child){
				var subsubtree = nested(child);
				children.push(subsubtree);
			});
      var substring = children.join();
      if(nest.hasOwnProperty('name')){
        subtree = "("+substring+")" + nest.name;
      }
      if(nest.hasOwnProperty('branch_length')){
        subtree = subtree + ":"+nest.branch_length;
      }
		}
		else{
      var leaf = "";
      if(nest.hasOwnProperty('name')){
        leaf = nest.name;
      }
      if(nest.hasOwnProperty('branch_length')){
        leaf = leaf + ":"+nest.branch_length;
      }
      subtree = subtree + leaf;
		}
		return subtree;
	}
	return nested(json) +";";
};

},{}],4:[function(require,module,exports){
//     Underscore.js 1.8.3
//     http://underscorejs.org
//     (c) 2009-2015 Jeremy Ashkenas, DocumentCloud and Investigative Reporters & Editors
//     Underscore may be freely distributed under the MIT license.

(function() {

  // Baseline setup
  // --------------

  // Establish the root object, `window` in the browser, or `exports` on the server.
  var root = this;

  // Save the previous value of the `_` variable.
  var previousUnderscore = root._;

  // Save bytes in the minified (but not gzipped) version:
  var ArrayProto = Array.prototype, ObjProto = Object.prototype, FuncProto = Function.prototype;

  // Create quick reference variables for speed access to core prototypes.
  var
    push             = ArrayProto.push,
    slice            = ArrayProto.slice,
    toString         = ObjProto.toString,
    hasOwnProperty   = ObjProto.hasOwnProperty;

  // All **ECMAScript 5** native function implementations that we hope to use
  // are declared here.
  var
    nativeIsArray      = Array.isArray,
    nativeKeys         = Object.keys,
    nativeBind         = FuncProto.bind,
    nativeCreate       = Object.create;

  // Naked function reference for surrogate-prototype-swapping.
  var Ctor = function(){};

  // Create a safe reference to the Underscore object for use below.
  var _ = function(obj) {
    if (obj instanceof _) return obj;
    if (!(this instanceof _)) return new _(obj);
    this._wrapped = obj;
  };

  // Export the Underscore object for **Node.js**, with
  // backwards-compatibility for the old `require()` API. If we're in
  // the browser, add `_` as a global object.
  if (typeof exports !== 'undefined') {
    if (typeof module !== 'undefined' && module.exports) {
      exports = module.exports = _;
    }
    exports._ = _;
  } else {
    root._ = _;
  }

  // Current version.
  _.VERSION = '1.8.3';

  // Internal function that returns an efficient (for current engines) version
  // of the passed-in callback, to be repeatedly applied in other Underscore
  // functions.
  var optimizeCb = function(func, context, argCount) {
    if (context === void 0) return func;
    switch (argCount == null ? 3 : argCount) {
      case 1: return function(value) {
        return func.call(context, value);
      };
      case 2: return function(value, other) {
        return func.call(context, value, other);
      };
      case 3: return function(value, index, collection) {
        return func.call(context, value, index, collection);
      };
      case 4: return function(accumulator, value, index, collection) {
        return func.call(context, accumulator, value, index, collection);
      };
    }
    return function() {
      return func.apply(context, arguments);
    };
  };

  // A mostly-internal function to generate callbacks that can be applied
  // to each element in a collection, returning the desired result — either
  // identity, an arbitrary callback, a property matcher, or a property accessor.
  var cb = function(value, context, argCount) {
    if (value == null) return _.identity;
    if (_.isFunction(value)) return optimizeCb(value, context, argCount);
    if (_.isObject(value)) return _.matcher(value);
    return _.property(value);
  };
  _.iteratee = function(value, context) {
    return cb(value, context, Infinity);
  };

  // An internal function for creating assigner functions.
  var createAssigner = function(keysFunc, undefinedOnly) {
    return function(obj) {
      var length = arguments.length;
      if (length < 2 || obj == null) return obj;
      for (var index = 1; index < length; index++) {
        var source = arguments[index],
            keys = keysFunc(source),
            l = keys.length;
        for (var i = 0; i < l; i++) {
          var key = keys[i];
          if (!undefinedOnly || obj[key] === void 0) obj[key] = source[key];
        }
      }
      return obj;
    };
  };

  // An internal function for creating a new object that inherits from another.
  var baseCreate = function(prototype) {
    if (!_.isObject(prototype)) return {};
    if (nativeCreate) return nativeCreate(prototype);
    Ctor.prototype = prototype;
    var result = new Ctor;
    Ctor.prototype = null;
    return result;
  };

  var property = function(key) {
    return function(obj) {
      return obj == null ? void 0 : obj[key];
    };
  };

  // Helper for collection methods to determine whether a collection
  // should be iterated as an array or as an object
  // Related: http://people.mozilla.org/~jorendorff/es6-draft.html#sec-tolength
  // Avoids a very nasty iOS 8 JIT bug on ARM-64. #2094
  var MAX_ARRAY_INDEX = Math.pow(2, 53) - 1;
  var getLength = property('length');
  var isArrayLike = function(collection) {
    var length = getLength(collection);
    return typeof length == 'number' && length >= 0 && length <= MAX_ARRAY_INDEX;
  };

  // Collection Functions
  // --------------------

  // The cornerstone, an `each` implementation, aka `forEach`.
  // Handles raw objects in addition to array-likes. Treats all
  // sparse array-likes as if they were dense.
  _.each = _.forEach = function(obj, iteratee, context) {
    iteratee = optimizeCb(iteratee, context);
    var i, length;
    if (isArrayLike(obj)) {
      for (i = 0, length = obj.length; i < length; i++) {
        iteratee(obj[i], i, obj);
      }
    } else {
      var keys = _.keys(obj);
      for (i = 0, length = keys.length; i < length; i++) {
        iteratee(obj[keys[i]], keys[i], obj);
      }
    }
    return obj;
  };

  // Return the results of applying the iteratee to each element.
  _.map = _.collect = function(obj, iteratee, context) {
    iteratee = cb(iteratee, context);
    var keys = !isArrayLike(obj) && _.keys(obj),
        length = (keys || obj).length,
        results = Array(length);
    for (var index = 0; index < length; index++) {
      var currentKey = keys ? keys[index] : index;
      results[index] = iteratee(obj[currentKey], currentKey, obj);
    }
    return results;
  };

  // Create a reducing function iterating left or right.
  function createReduce(dir) {
    // Optimized iterator function as using arguments.length
    // in the main function will deoptimize the, see #1991.
    function iterator(obj, iteratee, memo, keys, index, length) {
      for (; index >= 0 && index < length; index += dir) {
        var currentKey = keys ? keys[index] : index;
        memo = iteratee(memo, obj[currentKey], currentKey, obj);
      }
      return memo;
    }

    return function(obj, iteratee, memo, context) {
      iteratee = optimizeCb(iteratee, context, 4);
      var keys = !isArrayLike(obj) && _.keys(obj),
          length = (keys || obj).length,
          index = dir > 0 ? 0 : length - 1;
      // Determine the initial value if none is provided.
      if (arguments.length < 3) {
        memo = obj[keys ? keys[index] : index];
        index += dir;
      }
      return iterator(obj, iteratee, memo, keys, index, length);
    };
  }

  // **Reduce** builds up a single result from a list of values, aka `inject`,
  // or `foldl`.
  _.reduce = _.foldl = _.inject = createReduce(1);

  // The right-associative version of reduce, also known as `foldr`.
  _.reduceRight = _.foldr = createReduce(-1);

  // Return the first value which passes a truth test. Aliased as `detect`.
  _.find = _.detect = function(obj, predicate, context) {
    var key;
    if (isArrayLike(obj)) {
      key = _.findIndex(obj, predicate, context);
    } else {
      key = _.findKey(obj, predicate, context);
    }
    if (key !== void 0 && key !== -1) return obj[key];
  };

  // Return all the elements that pass a truth test.
  // Aliased as `select`.
  _.filter = _.select = function(obj, predicate, context) {
    var results = [];
    predicate = cb(predicate, context);
    _.each(obj, function(value, index, list) {
      if (predicate(value, index, list)) results.push(value);
    });
    return results;
  };

  // Return all the elements for which a truth test fails.
  _.reject = function(obj, predicate, context) {
    return _.filter(obj, _.negate(cb(predicate)), context);
  };

  // Determine whether all of the elements match a truth test.
  // Aliased as `all`.
  _.every = _.all = function(obj, predicate, context) {
    predicate = cb(predicate, context);
    var keys = !isArrayLike(obj) && _.keys(obj),
        length = (keys || obj).length;
    for (var index = 0; index < length; index++) {
      var currentKey = keys ? keys[index] : index;
      if (!predicate(obj[currentKey], currentKey, obj)) return false;
    }
    return true;
  };

  // Determine if at least one element in the object matches a truth test.
  // Aliased as `any`.
  _.some = _.any = function(obj, predicate, context) {
    predicate = cb(predicate, context);
    var keys = !isArrayLike(obj) && _.keys(obj),
        length = (keys || obj).length;
    for (var index = 0; index < length; index++) {
      var currentKey = keys ? keys[index] : index;
      if (predicate(obj[currentKey], currentKey, obj)) return true;
    }
    return false;
  };

  // Determine if the array or object contains a given item (using `===`).
  // Aliased as `includes` and `include`.
  _.contains = _.includes = _.include = function(obj, item, fromIndex, guard) {
    if (!isArrayLike(obj)) obj = _.values(obj);
    if (typeof fromIndex != 'number' || guard) fromIndex = 0;
    return _.indexOf(obj, item, fromIndex) >= 0;
  };

  // Invoke a method (with arguments) on every item in a collection.
  _.invoke = function(obj, method) {
    var args = slice.call(arguments, 2);
    var isFunc = _.isFunction(method);
    return _.map(obj, function(value) {
      var func = isFunc ? method : value[method];
      return func == null ? func : func.apply(value, args);
    });
  };

  // Convenience version of a common use case of `map`: fetching a property.
  _.pluck = function(obj, key) {
    return _.map(obj, _.property(key));
  };

  // Convenience version of a common use case of `filter`: selecting only objects
  // containing specific `key:value` pairs.
  _.where = function(obj, attrs) {
    return _.filter(obj, _.matcher(attrs));
  };

  // Convenience version of a common use case of `find`: getting the first object
  // containing specific `key:value` pairs.
  _.findWhere = function(obj, attrs) {
    return _.find(obj, _.matcher(attrs));
  };

  // Return the maximum element (or element-based computation).
  _.max = function(obj, iteratee, context) {
    var result = -Infinity, lastComputed = -Infinity,
        value, computed;
    if (iteratee == null && obj != null) {
      obj = isArrayLike(obj) ? obj : _.values(obj);
      for (var i = 0, length = obj.length; i < length; i++) {
        value = obj[i];
        if (value > result) {
          result = value;
        }
      }
    } else {
      iteratee = cb(iteratee, context);
      _.each(obj, function(value, index, list) {
        computed = iteratee(value, index, list);
        if (computed > lastComputed || computed === -Infinity && result === -Infinity) {
          result = value;
          lastComputed = computed;
        }
      });
    }
    return result;
  };

  // Return the minimum element (or element-based computation).
  _.min = function(obj, iteratee, context) {
    var result = Infinity, lastComputed = Infinity,
        value, computed;
    if (iteratee == null && obj != null) {
      obj = isArrayLike(obj) ? obj : _.values(obj);
      for (var i = 0, length = obj.length; i < length; i++) {
        value = obj[i];
        if (value < result) {
          result = value;
        }
      }
    } else {
      iteratee = cb(iteratee, context);
      _.each(obj, function(value, index, list) {
        computed = iteratee(value, index, list);
        if (computed < lastComputed || computed === Infinity && result === Infinity) {
          result = value;
          lastComputed = computed;
        }
      });
    }
    return result;
  };

  // Shuffle a collection, using the modern version of the
  // [Fisher-Yates shuffle](http://en.wikipedia.org/wiki/Fisher–Yates_shuffle).
  _.shuffle = function(obj) {
    var set = isArrayLike(obj) ? obj : _.values(obj);
    var length = set.length;
    var shuffled = Array(length);
    for (var index = 0, rand; index < length; index++) {
      rand = _.random(0, index);
      if (rand !== index) shuffled[index] = shuffled[rand];
      shuffled[rand] = set[index];
    }
    return shuffled;
  };

  // Sample **n** random values from a collection.
  // If **n** is not specified, returns a single random element.
  // The internal `guard` argument allows it to work with `map`.
  _.sample = function(obj, n, guard) {
    if (n == null || guard) {
      if (!isArrayLike(obj)) obj = _.values(obj);
      return obj[_.random(obj.length - 1)];
    }
    return _.shuffle(obj).slice(0, Math.max(0, n));
  };

  // Sort the object's values by a criterion produced by an iteratee.
  _.sortBy = function(obj, iteratee, context) {
    iteratee = cb(iteratee, context);
    return _.pluck(_.map(obj, function(value, index, list) {
      return {
        value: value,
        index: index,
        criteria: iteratee(value, index, list)
      };
    }).sort(function(left, right) {
      var a = left.criteria;
      var b = right.criteria;
      if (a !== b) {
        if (a > b || a === void 0) return 1;
        if (a < b || b === void 0) return -1;
      }
      return left.index - right.index;
    }), 'value');
  };

  // An internal function used for aggregate "group by" operations.
  var group = function(behavior) {
    return function(obj, iteratee, context) {
      var result = {};
      iteratee = cb(iteratee, context);
      _.each(obj, function(value, index) {
        var key = iteratee(value, index, obj);
        behavior(result, value, key);
      });
      return result;
    };
  };

  // Groups the object's values by a criterion. Pass either a string attribute
  // to group by, or a function that returns the criterion.
  _.groupBy = group(function(result, value, key) {
    if (_.has(result, key)) result[key].push(value); else result[key] = [value];
  });

  // Indexes the object's values by a criterion, similar to `groupBy`, but for
  // when you know that your index values will be unique.
  _.indexBy = group(function(result, value, key) {
    result[key] = value;
  });

  // Counts instances of an object that group by a certain criterion. Pass
  // either a string attribute to count by, or a function that returns the
  // criterion.
  _.countBy = group(function(result, value, key) {
    if (_.has(result, key)) result[key]++; else result[key] = 1;
  });

  // Safely create a real, live array from anything iterable.
  _.toArray = function(obj) {
    if (!obj) return [];
    if (_.isArray(obj)) return slice.call(obj);
    if (isArrayLike(obj)) return _.map(obj, _.identity);
    return _.values(obj);
  };

  // Return the number of elements in an object.
  _.size = function(obj) {
    if (obj == null) return 0;
    return isArrayLike(obj) ? obj.length : _.keys(obj).length;
  };

  // Split a collection into two arrays: one whose elements all satisfy the given
  // predicate, and one whose elements all do not satisfy the predicate.
  _.partition = function(obj, predicate, context) {
    predicate = cb(predicate, context);
    var pass = [], fail = [];
    _.each(obj, function(value, key, obj) {
      (predicate(value, key, obj) ? pass : fail).push(value);
    });
    return [pass, fail];
  };

  // Array Functions
  // ---------------

  // Get the first element of an array. Passing **n** will return the first N
  // values in the array. Aliased as `head` and `take`. The **guard** check
  // allows it to work with `_.map`.
  _.first = _.head = _.take = function(array, n, guard) {
    if (array == null) return void 0;
    if (n == null || guard) return array[0];
    return _.initial(array, array.length - n);
  };

  // Returns everything but the last entry of the array. Especially useful on
  // the arguments object. Passing **n** will return all the values in
  // the array, excluding the last N.
  _.initial = function(array, n, guard) {
    return slice.call(array, 0, Math.max(0, array.length - (n == null || guard ? 1 : n)));
  };

  // Get the last element of an array. Passing **n** will return the last N
  // values in the array.
  _.last = function(array, n, guard) {
    if (array == null) return void 0;
    if (n == null || guard) return array[array.length - 1];
    return _.rest(array, Math.max(0, array.length - n));
  };

  // Returns everything but the first entry of the array. Aliased as `tail` and `drop`.
  // Especially useful on the arguments object. Passing an **n** will return
  // the rest N values in the array.
  _.rest = _.tail = _.drop = function(array, n, guard) {
    return slice.call(array, n == null || guard ? 1 : n);
  };

  // Trim out all falsy values from an array.
  _.compact = function(array) {
    return _.filter(array, _.identity);
  };

  // Internal implementation of a recursive `flatten` function.
  var flatten = function(input, shallow, strict, startIndex) {
    var output = [], idx = 0;
    for (var i = startIndex || 0, length = getLength(input); i < length; i++) {
      var value = input[i];
      if (isArrayLike(value) && (_.isArray(value) || _.isArguments(value))) {
        //flatten current level of array or arguments object
        if (!shallow) value = flatten(value, shallow, strict);
        var j = 0, len = value.length;
        output.length += len;
        while (j < len) {
          output[idx++] = value[j++];
        }
      } else if (!strict) {
        output[idx++] = value;
      }
    }
    return output;
  };

  // Flatten out an array, either recursively (by default), or just one level.
  _.flatten = function(array, shallow) {
    return flatten(array, shallow, false);
  };

  // Return a version of the array that does not contain the specified value(s).
  _.without = function(array) {
    return _.difference(array, slice.call(arguments, 1));
  };

  // Produce a duplicate-free version of the array. If the array has already
  // been sorted, you have the option of using a faster algorithm.
  // Aliased as `unique`.
  _.uniq = _.unique = function(array, isSorted, iteratee, context) {
    if (!_.isBoolean(isSorted)) {
      context = iteratee;
      iteratee = isSorted;
      isSorted = false;
    }
    if (iteratee != null) iteratee = cb(iteratee, context);
    var result = [];
    var seen = [];
    for (var i = 0, length = getLength(array); i < length; i++) {
      var value = array[i],
          computed = iteratee ? iteratee(value, i, array) : value;
      if (isSorted) {
        if (!i || seen !== computed) result.push(value);
        seen = computed;
      } else if (iteratee) {
        if (!_.contains(seen, computed)) {
          seen.push(computed);
          result.push(value);
        }
      } else if (!_.contains(result, value)) {
        result.push(value);
      }
    }
    return result;
  };

  // Produce an array that contains the union: each distinct element from all of
  // the passed-in arrays.
  _.union = function() {
    return _.uniq(flatten(arguments, true, true));
  };

  // Produce an array that contains every item shared between all the
  // passed-in arrays.
  _.intersection = function(array) {
    var result = [];
    var argsLength = arguments.length;
    for (var i = 0, length = getLength(array); i < length; i++) {
      var item = array[i];
      if (_.contains(result, item)) continue;
      for (var j = 1; j < argsLength; j++) {
        if (!_.contains(arguments[j], item)) break;
      }
      if (j === argsLength) result.push(item);
    }
    return result;
  };

  // Take the difference between one array and a number of other arrays.
  // Only the elements present in just the first array will remain.
  _.difference = function(array) {
    var rest = flatten(arguments, true, true, 1);
    return _.filter(array, function(value){
      return !_.contains(rest, value);
    });
  };

  // Zip together multiple lists into a single array -- elements that share
  // an index go together.
  _.zip = function() {
    return _.unzip(arguments);
  };

  // Complement of _.zip. Unzip accepts an array of arrays and groups
  // each array's elements on shared indices
  _.unzip = function(array) {
    var length = array && _.max(array, getLength).length || 0;
    var result = Array(length);

    for (var index = 0; index < length; index++) {
      result[index] = _.pluck(array, index);
    }
    return result;
  };

  // Converts lists into objects. Pass either a single array of `[key, value]`
  // pairs, or two parallel arrays of the same length -- one of keys, and one of
  // the corresponding values.
  _.object = function(list, values) {
    var result = {};
    for (var i = 0, length = getLength(list); i < length; i++) {
      if (values) {
        result[list[i]] = values[i];
      } else {
        result[list[i][0]] = list[i][1];
      }
    }
    return result;
  };

  // Generator function to create the findIndex and findLastIndex functions
  function createPredicateIndexFinder(dir) {
    return function(array, predicate, context) {
      predicate = cb(predicate, context);
      var length = getLength(array);
      var index = dir > 0 ? 0 : length - 1;
      for (; index >= 0 && index < length; index += dir) {
        if (predicate(array[index], index, array)) return index;
      }
      return -1;
    };
  }

  // Returns the first index on an array-like that passes a predicate test
  _.findIndex = createPredicateIndexFinder(1);
  _.findLastIndex = createPredicateIndexFinder(-1);

  // Use a comparator function to figure out the smallest index at which
  // an object should be inserted so as to maintain order. Uses binary search.
  _.sortedIndex = function(array, obj, iteratee, context) {
    iteratee = cb(iteratee, context, 1);
    var value = iteratee(obj);
    var low = 0, high = getLength(array);
    while (low < high) {
      var mid = Math.floor((low + high) / 2);
      if (iteratee(array[mid]) < value) low = mid + 1; else high = mid;
    }
    return low;
  };

  // Generator function to create the indexOf and lastIndexOf functions
  function createIndexFinder(dir, predicateFind, sortedIndex) {
    return function(array, item, idx) {
      var i = 0, length = getLength(array);
      if (typeof idx == 'number') {
        if (dir > 0) {
            i = idx >= 0 ? idx : Math.max(idx + length, i);
        } else {
            length = idx >= 0 ? Math.min(idx + 1, length) : idx + length + 1;
        }
      } else if (sortedIndex && idx && length) {
        idx = sortedIndex(array, item);
        return array[idx] === item ? idx : -1;
      }
      if (item !== item) {
        idx = predicateFind(slice.call(array, i, length), _.isNaN);
        return idx >= 0 ? idx + i : -1;
      }
      for (idx = dir > 0 ? i : length - 1; idx >= 0 && idx < length; idx += dir) {
        if (array[idx] === item) return idx;
      }
      return -1;
    };
  }

  // Return the position of the first occurrence of an item in an array,
  // or -1 if the item is not included in the array.
  // If the array is large and already in sort order, pass `true`
  // for **isSorted** to use binary search.
  _.indexOf = createIndexFinder(1, _.findIndex, _.sortedIndex);
  _.lastIndexOf = createIndexFinder(-1, _.findLastIndex);

  // Generate an integer Array containing an arithmetic progression. A port of
  // the native Python `range()` function. See
  // [the Python documentation](http://docs.python.org/library/functions.html#range).
  _.range = function(start, stop, step) {
    if (stop == null) {
      stop = start || 0;
      start = 0;
    }
    step = step || 1;

    var length = Math.max(Math.ceil((stop - start) / step), 0);
    var range = Array(length);

    for (var idx = 0; idx < length; idx++, start += step) {
      range[idx] = start;
    }

    return range;
  };

  // Function (ahem) Functions
  // ------------------

  // Determines whether to execute a function as a constructor
  // or a normal function with the provided arguments
  var executeBound = function(sourceFunc, boundFunc, context, callingContext, args) {
    if (!(callingContext instanceof boundFunc)) return sourceFunc.apply(context, args);
    var self = baseCreate(sourceFunc.prototype);
    var result = sourceFunc.apply(self, args);
    if (_.isObject(result)) return result;
    return self;
  };

  // Create a function bound to a given object (assigning `this`, and arguments,
  // optionally). Delegates to **ECMAScript 5**'s native `Function.bind` if
  // available.
  _.bind = function(func, context) {
    if (nativeBind && func.bind === nativeBind) return nativeBind.apply(func, slice.call(arguments, 1));
    if (!_.isFunction(func)) throw new TypeError('Bind must be called on a function');
    var args = slice.call(arguments, 2);
    var bound = function() {
      return executeBound(func, bound, context, this, args.concat(slice.call(arguments)));
    };
    return bound;
  };

  // Partially apply a function by creating a version that has had some of its
  // arguments pre-filled, without changing its dynamic `this` context. _ acts
  // as a placeholder, allowing any combination of arguments to be pre-filled.
  _.partial = function(func) {
    var boundArgs = slice.call(arguments, 1);
    var bound = function() {
      var position = 0, length = boundArgs.length;
      var args = Array(length);
      for (var i = 0; i < length; i++) {
        args[i] = boundArgs[i] === _ ? arguments[position++] : boundArgs[i];
      }
      while (position < arguments.length) args.push(arguments[position++]);
      return executeBound(func, bound, this, this, args);
    };
    return bound;
  };

  // Bind a number of an object's methods to that object. Remaining arguments
  // are the method names to be bound. Useful for ensuring that all callbacks
  // defined on an object belong to it.
  _.bindAll = function(obj) {
    var i, length = arguments.length, key;
    if (length <= 1) throw new Error('bindAll must be passed function names');
    for (i = 1; i < length; i++) {
      key = arguments[i];
      obj[key] = _.bind(obj[key], obj);
    }
    return obj;
  };

  // Memoize an expensive function by storing its results.
  _.memoize = function(func, hasher) {
    var memoize = function(key) {
      var cache = memoize.cache;
      var address = '' + (hasher ? hasher.apply(this, arguments) : key);
      if (!_.has(cache, address)) cache[address] = func.apply(this, arguments);
      return cache[address];
    };
    memoize.cache = {};
    return memoize;
  };

  // Delays a function for the given number of milliseconds, and then calls
  // it with the arguments supplied.
  _.delay = function(func, wait) {
    var args = slice.call(arguments, 2);
    return setTimeout(function(){
      return func.apply(null, args);
    }, wait);
  };

  // Defers a function, scheduling it to run after the current call stack has
  // cleared.
  _.defer = _.partial(_.delay, _, 1);

  // Returns a function, that, when invoked, will only be triggered at most once
  // during a given window of time. Normally, the throttled function will run
  // as much as it can, without ever going more than once per `wait` duration;
  // but if you'd like to disable the execution on the leading edge, pass
  // `{leading: false}`. To disable execution on the trailing edge, ditto.
  _.throttle = function(func, wait, options) {
    var context, args, result;
    var timeout = null;
    var previous = 0;
    if (!options) options = {};
    var later = function() {
      previous = options.leading === false ? 0 : _.now();
      timeout = null;
      result = func.apply(context, args);
      if (!timeout) context = args = null;
    };
    return function() {
      var now = _.now();
      if (!previous && options.leading === false) previous = now;
      var remaining = wait - (now - previous);
      context = this;
      args = arguments;
      if (remaining <= 0 || remaining > wait) {
        if (timeout) {
          clearTimeout(timeout);
          timeout = null;
        }
        previous = now;
        result = func.apply(context, args);
        if (!timeout) context = args = null;
      } else if (!timeout && options.trailing !== false) {
        timeout = setTimeout(later, remaining);
      }
      return result;
    };
  };

  // Returns a function, that, as long as it continues to be invoked, will not
  // be triggered. The function will be called after it stops being called for
  // N milliseconds. If `immediate` is passed, trigger the function on the
  // leading edge, instead of the trailing.
  _.debounce = function(func, wait, immediate) {
    var timeout, args, context, timestamp, result;

    var later = function() {
      var last = _.now() - timestamp;

      if (last < wait && last >= 0) {
        timeout = setTimeout(later, wait - last);
      } else {
        timeout = null;
        if (!immediate) {
          result = func.apply(context, args);
          if (!timeout) context = args = null;
        }
      }
    };

    return function() {
      context = this;
      args = arguments;
      timestamp = _.now();
      var callNow = immediate && !timeout;
      if (!timeout) timeout = setTimeout(later, wait);
      if (callNow) {
        result = func.apply(context, args);
        context = args = null;
      }

      return result;
    };
  };

  // Returns the first function passed as an argument to the second,
  // allowing you to adjust arguments, run code before and after, and
  // conditionally execute the original function.
  _.wrap = function(func, wrapper) {
    return _.partial(wrapper, func);
  };

  // Returns a negated version of the passed-in predicate.
  _.negate = function(predicate) {
    return function() {
      return !predicate.apply(this, arguments);
    };
  };

  // Returns a function that is the composition of a list of functions, each
  // consuming the return value of the function that follows.
  _.compose = function() {
    var args = arguments;
    var start = args.length - 1;
    return function() {
      var i = start;
      var result = args[start].apply(this, arguments);
      while (i--) result = args[i].call(this, result);
      return result;
    };
  };

  // Returns a function that will only be executed on and after the Nth call.
  _.after = function(times, func) {
    return function() {
      if (--times < 1) {
        return func.apply(this, arguments);
      }
    };
  };

  // Returns a function that will only be executed up to (but not including) the Nth call.
  _.before = function(times, func) {
    var memo;
    return function() {
      if (--times > 0) {
        memo = func.apply(this, arguments);
      }
      if (times <= 1) func = null;
      return memo;
    };
  };

  // Returns a function that will be executed at most one time, no matter how
  // often you call it. Useful for lazy initialization.
  _.once = _.partial(_.before, 2);

  // Object Functions
  // ----------------

  // Keys in IE < 9 that won't be iterated by `for key in ...` and thus missed.
  var hasEnumBug = !{toString: null}.propertyIsEnumerable('toString');
  var nonEnumerableProps = ['valueOf', 'isPrototypeOf', 'toString',
                      'propertyIsEnumerable', 'hasOwnProperty', 'toLocaleString'];

  function collectNonEnumProps(obj, keys) {
    var nonEnumIdx = nonEnumerableProps.length;
    var constructor = obj.constructor;
    var proto = (_.isFunction(constructor) && constructor.prototype) || ObjProto;

    // Constructor is a special case.
    var prop = 'constructor';
    if (_.has(obj, prop) && !_.contains(keys, prop)) keys.push(prop);

    while (nonEnumIdx--) {
      prop = nonEnumerableProps[nonEnumIdx];
      if (prop in obj && obj[prop] !== proto[prop] && !_.contains(keys, prop)) {
        keys.push(prop);
      }
    }
  }

  // Retrieve the names of an object's own properties.
  // Delegates to **ECMAScript 5**'s native `Object.keys`
  _.keys = function(obj) {
    if (!_.isObject(obj)) return [];
    if (nativeKeys) return nativeKeys(obj);
    var keys = [];
    for (var key in obj) if (_.has(obj, key)) keys.push(key);
    // Ahem, IE < 9.
    if (hasEnumBug) collectNonEnumProps(obj, keys);
    return keys;
  };

  // Retrieve all the property names of an object.
  _.allKeys = function(obj) {
    if (!_.isObject(obj)) return [];
    var keys = [];
    for (var key in obj) keys.push(key);
    // Ahem, IE < 9.
    if (hasEnumBug) collectNonEnumProps(obj, keys);
    return keys;
  };

  // Retrieve the values of an object's properties.
  _.values = function(obj) {
    var keys = _.keys(obj);
    var length = keys.length;
    var values = Array(length);
    for (var i = 0; i < length; i++) {
      values[i] = obj[keys[i]];
    }
    return values;
  };

  // Returns the results of applying the iteratee to each element of the object
  // In contrast to _.map it returns an object
  _.mapObject = function(obj, iteratee, context) {
    iteratee = cb(iteratee, context);
    var keys =  _.keys(obj),
          length = keys.length,
          results = {},
          currentKey;
      for (var index = 0; index < length; index++) {
        currentKey = keys[index];
        results[currentKey] = iteratee(obj[currentKey], currentKey, obj);
      }
      return results;
  };

  // Convert an object into a list of `[key, value]` pairs.
  _.pairs = function(obj) {
    var keys = _.keys(obj);
    var length = keys.length;
    var pairs = Array(length);
    for (var i = 0; i < length; i++) {
      pairs[i] = [keys[i], obj[keys[i]]];
    }
    return pairs;
  };

  // Invert the keys and values of an object. The values must be serializable.
  _.invert = function(obj) {
    var result = {};
    var keys = _.keys(obj);
    for (var i = 0, length = keys.length; i < length; i++) {
      result[obj[keys[i]]] = keys[i];
    }
    return result;
  };

  // Return a sorted list of the function names available on the object.
  // Aliased as `methods`
  _.functions = _.methods = function(obj) {
    var names = [];
    for (var key in obj) {
      if (_.isFunction(obj[key])) names.push(key);
    }
    return names.sort();
  };

  // Extend a given object with all the properties in passed-in object(s).
  _.extend = createAssigner(_.allKeys);

  // Assigns a given object with all the own properties in the passed-in object(s)
  // (https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/Object/assign)
  _.extendOwn = _.assign = createAssigner(_.keys);

  // Returns the first key on an object that passes a predicate test
  _.findKey = function(obj, predicate, context) {
    predicate = cb(predicate, context);
    var keys = _.keys(obj), key;
    for (var i = 0, length = keys.length; i < length; i++) {
      key = keys[i];
      if (predicate(obj[key], key, obj)) return key;
    }
  };

  // Return a copy of the object only containing the whitelisted properties.
  _.pick = function(object, oiteratee, context) {
    var result = {}, obj = object, iteratee, keys;
    if (obj == null) return result;
    if (_.isFunction(oiteratee)) {
      keys = _.allKeys(obj);
      iteratee = optimizeCb(oiteratee, context);
    } else {
      keys = flatten(arguments, false, false, 1);
      iteratee = function(value, key, obj) { return key in obj; };
      obj = Object(obj);
    }
    for (var i = 0, length = keys.length; i < length; i++) {
      var key = keys[i];
      var value = obj[key];
      if (iteratee(value, key, obj)) result[key] = value;
    }
    return result;
  };

   // Return a copy of the object without the blacklisted properties.
  _.omit = function(obj, iteratee, context) {
    if (_.isFunction(iteratee)) {
      iteratee = _.negate(iteratee);
    } else {
      var keys = _.map(flatten(arguments, false, false, 1), String);
      iteratee = function(value, key) {
        return !_.contains(keys, key);
      };
    }
    return _.pick(obj, iteratee, context);
  };

  // Fill in a given object with default properties.
  _.defaults = createAssigner(_.allKeys, true);

  // Creates an object that inherits from the given prototype object.
  // If additional properties are provided then they will be added to the
  // created object.
  _.create = function(prototype, props) {
    var result = baseCreate(prototype);
    if (props) _.extendOwn(result, props);
    return result;
  };

  // Create a (shallow-cloned) duplicate of an object.
  _.clone = function(obj) {
    if (!_.isObject(obj)) return obj;
    return _.isArray(obj) ? obj.slice() : _.extend({}, obj);
  };

  // Invokes interceptor with the obj, and then returns obj.
  // The primary purpose of this method is to "tap into" a method chain, in
  // order to perform operations on intermediate results within the chain.
  _.tap = function(obj, interceptor) {
    interceptor(obj);
    return obj;
  };

  // Returns whether an object has a given set of `key:value` pairs.
  _.isMatch = function(object, attrs) {
    var keys = _.keys(attrs), length = keys.length;
    if (object == null) return !length;
    var obj = Object(object);
    for (var i = 0; i < length; i++) {
      var key = keys[i];
      if (attrs[key] !== obj[key] || !(key in obj)) return false;
    }
    return true;
  };


  // Internal recursive comparison function for `isEqual`.
  var eq = function(a, b, aStack, bStack) {
    // Identical objects are equal. `0 === -0`, but they aren't identical.
    // See the [Harmony `egal` proposal](http://wiki.ecmascript.org/doku.php?id=harmony:egal).
    if (a === b) return a !== 0 || 1 / a === 1 / b;
    // A strict comparison is necessary because `null == undefined`.
    if (a == null || b == null) return a === b;
    // Unwrap any wrapped objects.
    if (a instanceof _) a = a._wrapped;
    if (b instanceof _) b = b._wrapped;
    // Compare `[[Class]]` names.
    var className = toString.call(a);
    if (className !== toString.call(b)) return false;
    switch (className) {
      // Strings, numbers, regular expressions, dates, and booleans are compared by value.
      case '[object RegExp]':
      // RegExps are coerced to strings for comparison (Note: '' + /a/i === '/a/i')
      case '[object String]':
        // Primitives and their corresponding object wrappers are equivalent; thus, `"5"` is
        // equivalent to `new String("5")`.
        return '' + a === '' + b;
      case '[object Number]':
        // `NaN`s are equivalent, but non-reflexive.
        // Object(NaN) is equivalent to NaN
        if (+a !== +a) return +b !== +b;
        // An `egal` comparison is performed for other numeric values.
        return +a === 0 ? 1 / +a === 1 / b : +a === +b;
      case '[object Date]':
      case '[object Boolean]':
        // Coerce dates and booleans to numeric primitive values. Dates are compared by their
        // millisecond representations. Note that invalid dates with millisecond representations
        // of `NaN` are not equivalent.
        return +a === +b;
    }

    var areArrays = className === '[object Array]';
    if (!areArrays) {
      if (typeof a != 'object' || typeof b != 'object') return false;

      // Objects with different constructors are not equivalent, but `Object`s or `Array`s
      // from different frames are.
      var aCtor = a.constructor, bCtor = b.constructor;
      if (aCtor !== bCtor && !(_.isFunction(aCtor) && aCtor instanceof aCtor &&
                               _.isFunction(bCtor) && bCtor instanceof bCtor)
                          && ('constructor' in a && 'constructor' in b)) {
        return false;
      }
    }
    // Assume equality for cyclic structures. The algorithm for detecting cyclic
    // structures is adapted from ES 5.1 section 15.12.3, abstract operation `JO`.

    // Initializing stack of traversed objects.
    // It's done here since we only need them for objects and arrays comparison.
    aStack = aStack || [];
    bStack = bStack || [];
    var length = aStack.length;
    while (length--) {
      // Linear search. Performance is inversely proportional to the number of
      // unique nested structures.
      if (aStack[length] === a) return bStack[length] === b;
    }

    // Add the first object to the stack of traversed objects.
    aStack.push(a);
    bStack.push(b);

    // Recursively compare objects and arrays.
    if (areArrays) {
      // Compare array lengths to determine if a deep comparison is necessary.
      length = a.length;
      if (length !== b.length) return false;
      // Deep compare the contents, ignoring non-numeric properties.
      while (length--) {
        if (!eq(a[length], b[length], aStack, bStack)) return false;
      }
    } else {
      // Deep compare objects.
      var keys = _.keys(a), key;
      length = keys.length;
      // Ensure that both objects contain the same number of properties before comparing deep equality.
      if (_.keys(b).length !== length) return false;
      while (length--) {
        // Deep compare each member
        key = keys[length];
        if (!(_.has(b, key) && eq(a[key], b[key], aStack, bStack))) return false;
      }
    }
    // Remove the first object from the stack of traversed objects.
    aStack.pop();
    bStack.pop();
    return true;
  };

  // Perform a deep comparison to check if two objects are equal.
  _.isEqual = function(a, b) {
    return eq(a, b);
  };

  // Is a given array, string, or object empty?
  // An "empty" object has no enumerable own-properties.
  _.isEmpty = function(obj) {
    if (obj == null) return true;
    if (isArrayLike(obj) && (_.isArray(obj) || _.isString(obj) || _.isArguments(obj))) return obj.length === 0;
    return _.keys(obj).length === 0;
  };

  // Is a given value a DOM element?
  _.isElement = function(obj) {
    return !!(obj && obj.nodeType === 1);
  };

  // Is a given value an array?
  // Delegates to ECMA5's native Array.isArray
  _.isArray = nativeIsArray || function(obj) {
    return toString.call(obj) === '[object Array]';
  };

  // Is a given variable an object?
  _.isObject = function(obj) {
    var type = typeof obj;
    return type === 'function' || type === 'object' && !!obj;
  };

  // Add some isType methods: isArguments, isFunction, isString, isNumber, isDate, isRegExp, isError.
  _.each(['Arguments', 'Function', 'String', 'Number', 'Date', 'RegExp', 'Error'], function(name) {
    _['is' + name] = function(obj) {
      return toString.call(obj) === '[object ' + name + ']';
    };
  });

  // Define a fallback version of the method in browsers (ahem, IE < 9), where
  // there isn't any inspectable "Arguments" type.
  if (!_.isArguments(arguments)) {
    _.isArguments = function(obj) {
      return _.has(obj, 'callee');
    };
  }

  // Optimize `isFunction` if appropriate. Work around some typeof bugs in old v8,
  // IE 11 (#1621), and in Safari 8 (#1929).
  if (typeof /./ != 'function' && typeof Int8Array != 'object') {
    _.isFunction = function(obj) {
      return typeof obj == 'function' || false;
    };
  }

  // Is a given object a finite number?
  _.isFinite = function(obj) {
    return isFinite(obj) && !isNaN(parseFloat(obj));
  };

  // Is the given value `NaN`? (NaN is the only number which does not equal itself).
  _.isNaN = function(obj) {
    return _.isNumber(obj) && obj !== +obj;
  };

  // Is a given value a boolean?
  _.isBoolean = function(obj) {
    return obj === true || obj === false || toString.call(obj) === '[object Boolean]';
  };

  // Is a given value equal to null?
  _.isNull = function(obj) {
    return obj === null;
  };

  // Is a given variable undefined?
  _.isUndefined = function(obj) {
    return obj === void 0;
  };

  // Shortcut function for checking if an object has a given property directly
  // on itself (in other words, not on a prototype).
  _.has = function(obj, key) {
    return obj != null && hasOwnProperty.call(obj, key);
  };

  // Utility Functions
  // -----------------

  // Run Underscore.js in *noConflict* mode, returning the `_` variable to its
  // previous owner. Returns a reference to the Underscore object.
  _.noConflict = function() {
    root._ = previousUnderscore;
    return this;
  };

  // Keep the identity function around for default iteratees.
  _.identity = function(value) {
    return value;
  };

  // Predicate-generating functions. Often useful outside of Underscore.
  _.constant = function(value) {
    return function() {
      return value;
    };
  };

  _.noop = function(){};

  _.property = property;

  // Generates a function for a given object that returns a given property.
  _.propertyOf = function(obj) {
    return obj == null ? function(){} : function(key) {
      return obj[key];
    };
  };

  // Returns a predicate for checking whether an object has a given set of
  // `key:value` pairs.
  _.matcher = _.matches = function(attrs) {
    attrs = _.extendOwn({}, attrs);
    return function(obj) {
      return _.isMatch(obj, attrs);
    };
  };

  // Run a function **n** times.
  _.times = function(n, iteratee, context) {
    var accum = Array(Math.max(0, n));
    iteratee = optimizeCb(iteratee, context, 1);
    for (var i = 0; i < n; i++) accum[i] = iteratee(i);
    return accum;
  };

  // Return a random integer between min and max (inclusive).
  _.random = function(min, max) {
    if (max == null) {
      max = min;
      min = 0;
    }
    return min + Math.floor(Math.random() * (max - min + 1));
  };

  // A (possibly faster) way to get the current timestamp as an integer.
  _.now = Date.now || function() {
    return new Date().getTime();
  };

   // List of HTML entities for escaping.
  var escapeMap = {
    '&': '&amp;',
    '<': '&lt;',
    '>': '&gt;',
    '"': '&quot;',
    "'": '&#x27;',
    '`': '&#x60;'
  };
  var unescapeMap = _.invert(escapeMap);

  // Functions for escaping and unescaping strings to/from HTML interpolation.
  var createEscaper = function(map) {
    var escaper = function(match) {
      return map[match];
    };
    // Regexes for identifying a key that needs to be escaped
    var source = '(?:' + _.keys(map).join('|') + ')';
    var testRegexp = RegExp(source);
    var replaceRegexp = RegExp(source, 'g');
    return function(string) {
      string = string == null ? '' : '' + string;
      return testRegexp.test(string) ? string.replace(replaceRegexp, escaper) : string;
    };
  };
  _.escape = createEscaper(escapeMap);
  _.unescape = createEscaper(unescapeMap);

  // If the value of the named `property` is a function then invoke it with the
  // `object` as context; otherwise, return it.
  _.result = function(object, property, fallback) {
    var value = object == null ? void 0 : object[property];
    if (value === void 0) {
      value = fallback;
    }
    return _.isFunction(value) ? value.call(object) : value;
  };

  // Generate a unique integer id (unique within the entire client session).
  // Useful for temporary DOM ids.
  var idCounter = 0;
  _.uniqueId = function(prefix) {
    var id = ++idCounter + '';
    return prefix ? prefix + id : id;
  };

  // By default, Underscore uses ERB-style template delimiters, change the
  // following template settings to use alternative delimiters.
  _.templateSettings = {
    evaluate    : /<%([\s\S]+?)%>/g,
    interpolate : /<%=([\s\S]+?)%>/g,
    escape      : /<%-([\s\S]+?)%>/g
  };

  // When customizing `templateSettings`, if you don't want to define an
  // interpolation, evaluation or escaping regex, we need one that is
  // guaranteed not to match.
  var noMatch = /(.)^/;

  // Certain characters need to be escaped so that they can be put into a
  // string literal.
  var escapes = {
    "'":      "'",
    '\\':     '\\',
    '\r':     'r',
    '\n':     'n',
    '\u2028': 'u2028',
    '\u2029': 'u2029'
  };

  var escaper = /\\|'|\r|\n|\u2028|\u2029/g;

  var escapeChar = function(match) {
    return '\\' + escapes[match];
  };

  // JavaScript micro-templating, similar to John Resig's implementation.
  // Underscore templating handles arbitrary delimiters, preserves whitespace,
  // and correctly escapes quotes within interpolated code.
  // NB: `oldSettings` only exists for backwards compatibility.
  _.template = function(text, settings, oldSettings) {
    if (!settings && oldSettings) settings = oldSettings;
    settings = _.defaults({}, settings, _.templateSettings);

    // Combine delimiters into one regular expression via alternation.
    var matcher = RegExp([
      (settings.escape || noMatch).source,
      (settings.interpolate || noMatch).source,
      (settings.evaluate || noMatch).source
    ].join('|') + '|$', 'g');

    // Compile the template source, escaping string literals appropriately.
    var index = 0;
    var source = "__p+='";
    text.replace(matcher, function(match, escape, interpolate, evaluate, offset) {
      source += text.slice(index, offset).replace(escaper, escapeChar);
      index = offset + match.length;

      if (escape) {
        source += "'+\n((__t=(" + escape + "))==null?'':_.escape(__t))+\n'";
      } else if (interpolate) {
        source += "'+\n((__t=(" + interpolate + "))==null?'':__t)+\n'";
      } else if (evaluate) {
        source += "';\n" + evaluate + "\n__p+='";
      }

      // Adobe VMs need the match returned to produce the correct offest.
      return match;
    });
    source += "';\n";

    // If a variable is not specified, place data values in local scope.
    if (!settings.variable) source = 'with(obj||{}){\n' + source + '}\n';

    source = "var __t,__p='',__j=Array.prototype.join," +
      "print=function(){__p+=__j.call(arguments,'');};\n" +
      source + 'return __p;\n';

    try {
      var render = new Function(settings.variable || 'obj', '_', source);
    } catch (e) {
      e.source = source;
      throw e;
    }

    var template = function(data) {
      return render.call(this, data, _);
    };

    // Provide the compiled source as a convenience for precompilation.
    var argument = settings.variable || 'obj';
    template.source = 'function(' + argument + '){\n' + source + '}';

    return template;
  };

  // Add a "chain" function. Start chaining a wrapped Underscore object.
  _.chain = function(obj) {
    var instance = _(obj);
    instance._chain = true;
    return instance;
  };

  // OOP
  // ---------------
  // If Underscore is called as a function, it returns a wrapped object that
  // can be used OO-style. This wrapper holds altered versions of all the
  // underscore functions. Wrapped objects may be chained.

  // Helper function to continue chaining intermediate results.
  var result = function(instance, obj) {
    return instance._chain ? _(obj).chain() : obj;
  };

  // Add your own custom functions to the Underscore object.
  _.mixin = function(obj) {
    _.each(_.functions(obj), function(name) {
      var func = _[name] = obj[name];
      _.prototype[name] = function() {
        var args = [this._wrapped];
        push.apply(args, arguments);
        return result(this, func.apply(_, args));
      };
    });
  };

  // Add all of the Underscore functions to the wrapper object.
  _.mixin(_);

  // Add all mutator Array functions to the wrapper.
  _.each(['pop', 'push', 'reverse', 'shift', 'sort', 'splice', 'unshift'], function(name) {
    var method = ArrayProto[name];
    _.prototype[name] = function() {
      var obj = this._wrapped;
      method.apply(obj, arguments);
      if ((name === 'shift' || name === 'splice') && obj.length === 0) delete obj[0];
      return result(this, obj);
    };
  });

  // Add all accessor Array functions to the wrapper.
  _.each(['concat', 'join', 'slice'], function(name) {
    var method = ArrayProto[name];
    _.prototype[name] = function() {
      return result(this, method.apply(this._wrapped, arguments));
    };
  });

  // Extracts the result from a wrapped and chained object.
  _.prototype.value = function() {
    return this._wrapped;
  };

  // Provide unwrapping proxy for some methods used in engine operations
  // such as arithmetic and JSON stringification.
  _.prototype.valueOf = _.prototype.toJSON = _.prototype.value;

  _.prototype.toString = function() {
    return '' + this._wrapped;
  };

  // AMD registration happens at the end for compatibility with AMD loaders
  // that may not enforce next-turn semantics on modules. Even though general
  // practice for AMD registration is to be anonymous, underscore registers
  // as a named module because, like jQuery, it is a base library that is
  // popular enough to be bundled in a third party lib, but not be part of
  // an AMD load request. Those cases could generate an error when an
  // anonymous define() is called outside of a loader request.
  if (typeof define === 'function' && define.amd) {
    define('underscore', [], function() {
      return _;
    });
  }
}.call(this));

},{}],5:[function(require,module,exports){
(function(root) {
'use strict';

var U;
if (typeof _ === 'function') {
  U = _;
} else if (typeof require === 'function') {
  U = require('underscore');
} else {
  throw new Error("Cannot find or require underscore.js.");
}


  //////////////////////////
 //   Parsing VCF Files  //
//////////////////////////

// Versions we know we can parse
var ALLOWED_VERSIONS = ['VCFv4.0', 'VCFv4.1', 'VCFv4.2'];

// There are 9 columns before samples are listed.
var NUM_STANDARD_HEADER_COLUMNS = 9;

var BASES = ['A', 'C', 'T', 'G'];

// Console prints messages when deriving undefined types.
var WARN = false;

// VCF values can be comma-separated lists, so here we want to convert them into
// lists of the proper type, else return the singleton value of that type. All
// values are also nullable, signified with a '.', we want to watch out for
// those as well.
function maybeMapOverVal(fn, val) {
  var vals = val.split(',');
  if (vals.length > 1) {
    return vals.map(function(v) { return v == '.' ? null : fn(v); });
  }
  return val == '.' ? null : fn(val);
}

// Set radix to 10 to prevent problems in strangely-formed VCFs. Otherwise we
// may end up parsing octal, for example, if the int starts with a 0.
// c.f. http://stackoverflow.com/questions/7818903/jslint-says-missing-radix-parameter-what-should-i-do
var _parseInt = function(i) { return parseInt(i, 10); };

// This map associates types with functions to parse from strings the associated
// JS types.
var HEADER_TYPES = {'Integer': U.partial(maybeMapOverVal, _parseInt),
                    'Float': U.partial(maybeMapOverVal, parseFloat),
                    'Flag': U.constant(true),
                    'Character': U.partial(maybeMapOverVal, U.identity),
                    'String': function(v) { return v == '.' ? null : v; }};

function deriveType(val) {
  // Returns the derived type, Flag or Float, falling back to String if nothing
  // else works. Used when the type isn't specified in the header.
  if (!val || val.length === 0) {
    return 'Flag';
  } else if (!U.isNaN(parseFloat(val))) {
    return 'Float';
  } else {
    return 'String';
  }
}


  ////////////////////////////////////////////////////////
 // Below: parsing the header metadata of a VCF file.  //
////////////////////////////////////////////////////////

function parseHeader(headers) {
  // Returns a header object with keys header line types (.e.g format, info,
  // sample) and values arrays of objects containing the key-value information
  // stored therein.
  if (headers.length === 0) {
    throw new Error('Invalid VCF File: missing header.');
  }

  var header = {};
  header.__RAW__ = headers;
  // VCF header lines always start with either one or two # signs.
  headers = U.map(headers, function(h) { return h.replace(/^##?/, ""); });

  header.columns = headers[headers.length-1].split('\t');
  header.sampleNames = header.columns.slice(NUM_STANDARD_HEADER_COLUMNS);

  // TODO(ihodes): parse other, less frequently used, header lines like
  //               'assembly', 'contig'
  header.VERSION = parseVCFVersion(headers);
  header.ALT = parseHeadersOf('ALT', headers);
  header.INFO = parseHeadersOf('INFO', headers);
  header.FORMAT = parseHeadersOf('FORMAT', headers);
  header.SAMPL = parseHeadersOf('SAMPLE', headers);
  header.PEDIGREE = parseHeadersOf('PEDIGREE', headers);

  return header;
}

function headersOf(type, lines) {
  // Returns the header lines out of `lines` matching `type`.
  return U.filter(lines, function(h) {
    return h.substr(0, type.length) == type;
  });
}

function parseHeaderLines(lines) {
  // Returns a list of parsed header lines.
  //
  // `lines` - an array of header strings (stripped of "##").
  return U.reduce(lines, function(headers, line) {
    var specRe = /<(.*)>/,
        descriptionRe = /.*?(Description="(.*?)",?).*?/,
        spec = specRe.exec(line)[1],
        descriptionExec = descriptionRe.exec(spec)
          
    if (descriptionExec) {
      var description = descriptionExec[2];
    }

    spec = spec.replace(/Description=".*?",?/, '');

    var kvs = U.map(spec.split(','), function(kv) {
      return kv.split('=');
    });

    if (description)  kvs.push(['Description', description]);

    headers.push(U.reduce(kvs, function(acc, kv){
      var val = kv[1],
          key = kv[0];
      if (key.length <= 0)  return acc;

      if (val === '.')  val = null;
      else if (key === 'Number')  val = parseInt(val);
      acc[key] = val;
      return acc;
    }, {}));
    return headers;
  }, []);
}

function parseVCFVersion(headers) {
  // Returns the version of the VCF file. Hacky.
  var version = headers[0].split('=')[1];
  if (!U.contains(ALLOWED_VERSIONS, version)) {
    throw new Error("Invalid VCF File: version must be 4.2, 4.1, or 4.0.");
  }
  return '4.1';
}

function parseHeadersOf(type, headers) {
  // Returns a list of parsed headers for a given type.
  //
  // `type` - String, type (e.g. ALT) of header line.
  // `headers` - List of split header strings.
  return parseHeaderLines(headersOf(type, headers));
}


  ////////////////////////////////////////////////////////
 // Below: parsing columns of individual VCF records.  //
////////////////////////////////////////////////////////
function _parseChrom(chrom, header) {
  return chrom;
}

function _parsePos(pos, header) {
  return parseInt(pos);
}

function _parseId(id, header) {
  return id.split(';');
}

function _parseRef(ref, header) {
  return ref;
}

function _parseAlt(alt, header) {
  return alt.split(',');
}

function _parseQual(qual, header) {
  return parseFloat(qual);
}

function _parseFilter(filters, header) {
  return filters.split(';');
}

function _parseInfo(info, header) {
  var infos = info.split(';'),
      parsedInfo = {};
  for (var idx = 0; idx < infos.length; idx++) {
    var kv = infos[idx].split('='),
        key = kv[0],
        val = kv[1],
        headerSpec,
        type;

    // Optimization for _.findWhere(header.INFO, {ID: key})
    var hspec, hinfo = header.INFO;
    for(var i = 0; i < hinfo.length; i++) {
      hspec = hinfo[i];
      if (hspec.ID == key) headerSpec = hspec;
    }

    if (headerSpec && headerSpec.Type) {
      type = headerSpec.Type;
    } else {
      type = deriveType(val);
      if (WARN) console.warn("INFO type '" + key + "' is not defined in header. (Value = '" + val + "'). Derived type as '" + type + "'.");
    }

    val = HEADER_TYPES[type](val);
    parsedInfo[key] = val;
  }
  return parsedInfo;
}

function _parseFormat(format, header) {
  // Returns a list of format tags.
  return format.split(':');
}

function _parseSample(sample, format, header) {
  var sampleVals = sample.split(':'),
      parsedSample = {};
  for (var idx = 0; idx < sampleVals.length; idx++) {
    var val = sampleVals[idx],
        key = format[idx],
        headerSpec,
        type;

    // The below is a massive optimization for:
    // headerSpec = _.findWhere(header.FORMAT, {ID: key})
    var hspec, hfmt = header.FORMAT;
    for(var fi = 0; fi < hfmt.length; fi++) {
      hspec = hfmt[fi];
      if (hspec.ID == key) headerSpec = hspec;
    }

    if (headerSpec && headerSpec.Type) {
      type = headerSpec.Type;
    } else {
      type = deriveType(val);
      if (WARN) console.warn("INFO type '" + key + "' is not defined in header. (Value = '" + val + "'). Derived type as '" + type + "'.");
    }
    val = HEADER_TYPES[type](val);
    parsedSample[key] = val;
  }
  return parsedSample;
}

function _genKey(record) {
  return record.CHROM + ':' + record.POS + "(" + record.REF + "->" + record.ALT + ")";
}


function parser() {
  var data = {},
      header = [],
      parseChrom = _parseChrom,
      parsePos = _parsePos,
      parseId = _parseId,
      parseRef = _parseRef,
      parseAlt = _parseAlt,
      parseQual = _parseQual,
      parseFilter = _parseFilter,
      parseInfo = _parseInfo,
      parseFormat = _parseFormat,
      parseSample = _parseSample,
      genKey = _genKey;


  function _parser(text) {
    var parsedVcf = parseVCF(text);
    return {records: parsedVcf.records,
            header: parsedVcf.header};
  }

    //////////////////////////////////////////////////////////////////////
   // Below: initializing Records and parsing their constituent data.  //
  //////////////////////////////////////////////////////////////////////

  function Record(line, header) {
    // Returns a VCF record.
    //
    // `line` - a line of the VCF file that represents an individual record.
    // `header` - the parsed VCF header.
    var vals = line.split('\t');
    this.initializeRecord(vals, header);

    if (this.CHROM)   this.CHROM   = parseChrom(this.CHROM, header);
    if (this.POS)     this.POS     = parsePos(this.POS, header);
    if (this.ID)      this.ID      = parseId(this.ID, header);
    if (this.REF)     this.REF     = parseRef(this.REF, header);
    if (this.ALT)     this.ALT     = parseAlt(this.ALT, header);
    if (this.QUAL)    this.QUAL    = parseQual(this.QUAL, header);
    if (this.FILTER)  this.FILTER  = parseFilter(this.FILTER, header);
    if (this.INFO)    this.INFO    = parseInfo(this.INFO, header);
    if (this.FORMAT)  this.FORMAT  = parseFormat(this.FORMAT, header);
    this.__KEY__ = genKey(this, header);

    for (var idx = 0; idx < header.sampleNames.length; idx++) {
      var sampleName = header.sampleNames[idx],
          sample = this[sampleName];
      if (sample) {
        this[sampleName] = parseSample(sample, this.FORMAT, header);
      }
    }
  }

  Record.prototype.initializeRecord = function(vals, header) {
    this.__HEADER__ = header;
    for (var idx = 0; idx < header.columns.length; idx++) {
      var colname = header.columns[idx],
          val = vals[idx].trim();
      if (!val || val === '.') val = null;
      this[colname] = val;
    }
  };

  Record.prototype.variantType = function() {
    if (this.isSnv()) return 'SNV';
    if (this.isSv()) return 'SV';
    if (this.isIndel()) return 'INDEL';
    return null;
  };

  Record.prototype.isSnv = function() {
    var isSnv = true;
    if (this.REF && this.REF.length > 1) isSnv = false;
    U.each(this.ALT, function(alt) {
      if (alt && !U.contains(BASES, alt)) isSnv = false;
    });
    return isSnv;
  };

  Record.prototype.isSv = function() {
    if (this.INFO && this.INFO.SVTYPE) return true;
    return false;
  };

  Record.prototype.isCnv = function() {
    if (this.INFO && this.INFO.SVTYPE === 'CNV') return true;
    return false;
  };

  Record.prototype.isIndel = function() {
    return this.isDeletion() || this.isInsertion();
  };

  Record.prototype.isDeletion = function() {
    if (this.isSv()) return false;
    if (this.ALT && this.ALT.length > 1) return false;
    if (this.REF && this.ALT && this.ALT.length <= 1) {
      if (this.REF.length > this.ALT[0].length) return true;
    }
    return false;
  };

  Record.prototype.isInsertion = function() {
    if (this.isSv()) return false;
    if (this.REF && this.ALT && this.ALT.length >= 1) {
      if (this.REF.length < this.ALT[0].length) return true;
    }
    return false;
  };


  // Returns a parsed VCF object, with attributes `records` and `header`.
  //    `records` - a list of VCF Records.
  //    `header` - an object of the metadata parsed from the VCF header.
  //
  // `text` - VCF plaintext.
  function parseVCF(text) {
    var lines = U.reject(text.split('\n'), function(line) {
      return line === '';
    });

    var partitions = U.partition(lines, function(line) {
      return line[0] === '#';
    });

    var header = parseHeader(partitions[0]),
        records = U.map(partitions[1], function(line) {
          return new Record(line, header);
        });

    return {header: header, records: records};
  }


    ///////////////////////////
   //   Primary VCF.js API  //
  ///////////////////////////

  _parser.parseChrom = function(_) {
    if (!arguments.length) return parseChrom;
    parseChrom = _;
    return _parser;
  };
  _parser.parsePos = function(_) {
    if (!arguments.length) return parsePos;
    parsePos = _;
    return _parser;
  };
  _parser.parseId = function(_) {
    if (!arguments.length) return parseId;
    parseId = _;
    return _parser;
  };
  _parser.parseRef = function(_) {
    if (!arguments.length) return parseRef;
    parseRef = _;
    return _parser;
  };
  _parser.parseAlt = function(_) {
    if (!arguments.length) return parseAlt;
    parseAlt = _;
    return _parser;
  };
  _parser.parseQual = function(_) {
    if (!arguments.length) return parseQual;
    parseQual = _;
    return _parser;
  };
  _parser.parseFilter = function(_) {
    if (!arguments.length) return parseFilter;
    parseFilter = _;
    return _parser;
  };
  _parser.parseInfo = function(_) {
    if (!arguments.length) return parseInfo;
    parseInfo = _;
    return _parser;
  };
  _parser.parseFormat = function(_) {
    if (!arguments.length) return parseFormat;
    parseFormat = _;
    return _parser;
  };
  _parser.parseSample = function(_) {
    if (!arguments.length) return parseSample;
    parseSample = _;
    return _parser;
  };
  _parser.genKey = function(_) {
    if (!arguments.length) return genKey;
    genKey = _;
    return _parser;
  };
  _parser.warn = function() {
    if (!arguments.length) return WARN;
    WARN = _;
    return _parser;
  };

  return _parser;
}


/**
 * Return list of records which fall between (or, in the case of a SNV, overlap)
 * start and end.
 *
 * O(N) time.
 */
function fetch(records, chromosome, start, end) {
  // TODO(ihodes): Add sorted option to get O(lnN), fallback to O(N).
  return U.filter(records, function(record) {
    if (record.CHROM === chromosome && record.POS < end) {
      if (record.POS >= start)
        return true;
      if (record.INFO && record.INFO.END && record.INFO.END >= start)
        return true;
    }
    return false;
  });
}


  ///////////////////////
 // Exporting the API //
///////////////////////

var exports = {
  parser: parser,
  fetch: fetch
};

if (typeof define === "function" && define.amd) {
  define(exports);
} else if (typeof module === "object" && module.exports) {
  module.exports = exports;
} else {
  root.vcf = exports;
}

})(this);

},{"underscore":4}],6:[function(require,module,exports){
(function() {
  "use strict";

  var vcf = require('vcf.js');

  function parseVCFText(text) {
    var parsed = vcf.parser()(text);
    return parsed;
    //console.log(parsed);
    //var inheritancePattern = parsed.records[0].INFO.DNT;
    //var ret = {};
    //ret.parent0 = inheritancePattern.slice(0, 2);
    //ret.parent1 = inheritancePattern.slice(3, 5);
    //ret.child = inheritancePattern.slice(6, 8);
    //return ret;
  }

  module.exports.parseVCFText = parseVCFText;

}());

},{"vcf.js":5}],7:[function(require,module,exports){
//var d3 = require('d3');
var vcfParser = require('./dng_vcf_parser');
var pedParser = require('./ped_parser');
var pedigr = require('./pedigr');

//var pedigreeFileText;
//var dngOutputFileText;

//TODO: clean this up
//jQuery.get('example_pedigree.ped', function(pedData) {
//    pedigreeFileText = pedData;
//
//    jQuery.get('example_output.vcf', function(outputData) {
//      dngOutputFileText = outputData;
//      main();
//    });
//});

main();

function main() {

  /*PEDIGREE_FILE_TEXT_PLACEHOLDER*/
  /*LAYOUT_DATA_PLACEHOLDER*/
  /*DNG_VCF_DATA_PLACEHOLDER*/

  //dngOutputData = dngOutputData.replace('\\\n', function() { return '\n'; });
  //dngOutputData = dngOutputData.replace('\"', function() { return '"'; });

  //d3.select('#pedigree_file_input').on('change', updatePedigreeFile);
  //d3.select('#dng_output_file_input').on('change', updateDNGOutputFile);

  var idText = d3.select('#id_display');
  var pedGraph = null;
  var activeNode = null;
  
  serverPedigreeAndLayout(function(nodes, links) {
    dngOverlay()
    doVisuals(nodes, links);
  });

  function dngOverlay() {
    var vcfData = vcfParser.parseVCFText(dngOutputFileText);
    //console.log(vcfData);

    var mutationLocation = vcfData.records[0].INFO.DNL;
    console.log(mutationLocation);
    var owner = findOwnerNode(mutationLocation);

    if (owner !== undefined) {
      var ownerParentageLink = owner.getParentageLink();
      var parentageLinkData = {
        mutation: vcfData.records[0].INFO.DNT
      };
      ownerParentageLink.setData(parentageLinkData);

      for (var sampleName of vcfData.header.sampleNames) {
        var format = vcfData.records[0][sampleName];

        if (isPersonNode(sampleName)) {
          var id = getIdFromSampleName(sampleName);
          var personNode = pedGraph.getPerson(id);
          personNode.data.dngOutputData = format;
        }
        else {
          var sampleNode = findMatchingSampleNode(sampleName);
          sampleNode.dngOutputData = format;
        }
      }
    }
    else {
      alert("No mutation found!");
    }
  }

  function serverPedigreeAndLayout(callback) {

    //var pedigreeUploadData = { text: pedigreeFileText };
    //jQuery.ajax('/pedigree_and_layout',
    //  { 
    //    type: 'POST',
    //    data: JSON.stringify(pedigreeUploadData),
    //    contentType: 'application/json',
    //    success: gotLayoutData
    //  });
    gotLayoutData();

    function gotLayoutData(jsonData) {
      //console.log(jsonData);
      //var layoutData = JSON.parse(jsonData);

      var pedigreeData = pedParser.parsePedigreeFile(pedigreeFileText);

      var ret = processPedigree(layoutData, pedigreeData);
      var nodes = ret.nodes;
      var links = ret.links;

      //doVisuals(nodes, links);

      callback(nodes, links);
    }
  }

  function doVisuals(nodes, links) {
      var height = 500;

      d3.select("svg").remove();

      var zoom = d3.zoom()
        .on("zoom", zoomed);


      var chartWrapper= d3.select("#chart_wrapper")
      var dim = chartWrapper.node().getBoundingClientRect();

      var svg = chartWrapper.append("svg")
          .attr("width", dim.width)
          .attr("height", height);

      var container = svg.call(zoom)
        .append("g");

        
      var link = container
        .append("g")
          .attr("class", "links")
        .selectAll("line")
        .data(links)
        .enter()
        .append("g")
          .attr("class", "link");

      link.append("line")
        .attr("stroke-width", function(d) {
          if (linkHasData(d)) {
              return 5;
          }
          return 1;
        })
        .attr("stroke", function(d) {
          if (linkHasData(d)) {
            return "green";
          }

          return "#999";
        })
        .attr("x1", function(d) { return d.source.x; })
        .attr("y1", function(d) { return d.source.y; })
        .attr("x2", function(d) { return d.target.x; })
        .attr("y2", function(d) { return d.target.y; });

      link.append("text")
        .attr("dx", function(d) {
          return halfwayBetweenGeneric(d.source.x, d.target.x) - 45;
        })
        .attr("dy", function(d) {
          return halfwayBetweenGeneric(d.source.y, d.target.y);
        })
        .text(function(d) {
          if (linkHasData(d)) {
            return d.dataLink.data.mutation;
          }
        });

      function linkHasData(d) {
          return d.dataLink !== undefined && d.dataLink.data !== undefined;
      }


      var node = container
        .append("g")
          .attr("class", "nodes")
        .selectAll(".node")
        .data(nodes)
        .enter()
        .append("g")
          .attr("class", "node");

      node.append("path")
        .attr("d", d3.symbol()
          .type(function(d) {
            if (d.type === 'person') {
              if (d.dataNode.sex === 'male') {
                return d3.symbolSquare;
              }
              else if (d.dataNode.sex === 'female') {
                return d3.symbolCircle;
              }
            }
            else {
              return d3.symbolTriangle;
            }
          })
          .size(500))
        .attr("fill", fillColor)
        .attr('opacity', function(d) {
          if (d.type === 'marriage') {
            return 0;
          }
          else {
            return 1;
          }
        })
        .on("click", nodeClicked);

      node.append("text")
        .attr("dx", 20)
        .attr("dy", ".35em")
        .text(function(d) { 
          if (d.type !== 'marriage') {
            return d.dataNode.id
          }
        });
      
      node.attr("transform", function(d) {
          return "translate(" + d.x + "," + d.y + ")";
      });

      function zoomed() {
        container.attr("transform", d3.event.transform);
      }

      function nodeClicked(d) {
        if (d.type !== 'marriage') {
          d3.select(activeNode).style('fill', fillColor);
          activeNode = this;
          d3.select(this).style('fill', 'DarkSeaGreen');

          if (d.dataNode.data.dngOutputData !== undefined) {
            document.getElementById('id_display').value =
              d.dataNode.id;
          }
          else {
            document.getElementById('id_display').value = "";
          }

        }
      }

      function fillColor(d) {
        if (d.type === 'person') {
          if (d.dataNode.sex === 'male') {
            return "SteelBlue";
          }
          else if (d.dataNode.sex === 'female') {
            return "Tomato";
          }
        }
        else {
          return "black";
        }
      }
  }

  function processPedigree(data, pedigreeData) {
    //console.log(pedigreeData);

    pedGraph = buildGraphFromPedigree(pedigreeData);

    //console.log(pedGraph);

    var layout = data.layout;
    var nodes = [];
    var links = [];

    // build person nodes
    layout.nid.forEach(function(row, rowIdx) {
      for (var colIdx = 0; colIdx < layout.n[rowIdx]; colIdx++) {

        var id = row[colIdx];

        var node = {};
        node.type = 'person';
        node.dataNode = pedGraph.getPerson(id);
        node.x = 80 * layout.pos[rowIdx][colIdx];
        node.y = 100 * rowIdx;

        // TODO: such a hack. remove
        node.rowIdx = rowIdx;
        node.colIdx = colIdx;

        nodes.push(node);
      }
    });

    // build marriage nodes and links
    nodes.forEach(function(node, index) {
      if (layout.spouse[node.rowIdx][node.colIdx] === 1) {
        var spouseNode = nodes[index + 1];

        var marriageNode = createMarriageNode(node, spouseNode);
        nodes.push(marriageNode);

        links.push(createMarriageLink(node, marriageNode));
        links.push(createMarriageLink(spouseNode, marriageNode));

        var marriage = pedigr.MarriageBuilder.createMarriageBuilder()
          .spouse(node.dataNode)
          .spouse(spouseNode.dataNode)
          .build();

        pedGraph.addMarriage(marriage);

        var children = getAllKids(data, nodes, node, spouseNode);

        for (var childNode of children) {
          var childLink = createChildLink(childNode, marriageNode);
          var parentageLink = marriage.addChild(childNode.dataNode);
          childLink.dataLink = parentageLink;
          links.push(childLink);
        }

      }

      // TODO: such a hack. remove
      delete node.rowIdx;
      delete node.colIdx;
    });

    return { nodes: nodes, links: links };
  }

  function buildGraphFromPedigree(pedigreeData) {
    var pedGraph = pedigr.PedigreeGraph.createGraph();

    pedigreeData.forEach(function(person) {
      var person = pedigr.PersonBuilder
        .createPersonBuilder(person.individualId)
          .sex(person.sex)
          .data({ sampleIds: person.sampleIds })
          .build();
      pedGraph.addPerson(person);
    });

    return pedGraph;
  }

  function findOwnerNode(sampleName) {
    var strippedName = getStrippedName(sampleName);
    for (var person of pedGraph.getPersons()) {
      console.log(person.data.sampleIds);
      var sampleNode = findInTree(person.data.sampleIds, strippedName);
      if (sampleNode !== undefined) {
        return person;
      }
    }
    return undefined;
  }

  function findMatchingSampleNode(sampleName) {
    var strippedName = getStrippedName(sampleName);
    for (var person of pedGraph.getPersons()) {
      var sampleNode = findInTree(person.data.sampleIds, strippedName);
      if (sampleNode !== undefined) {
        return sampleNode;
      }
    }
    return undefined;
  }

  function findInTree(tree, sampleName) {

    if (tree.name === sampleName) {
      return tree;
    }

    if (tree.children !== undefined) {
      if (tree.children.length === 0) {
        return undefined;
      }
      else {
        for (child of tree.children) {
          var inChild = findInTree(child, sampleName);
          if (inChild !== undefined) {
            return inChild;
          }
        }
      }
    }

    return undefined;
  }

  function getStrippedName(sampleName) {
    var stripped = sampleName.slice(3, sampleName.indexOf(':'));
    return stripped;
  }

  function isPersonNode(sampleName) {
    return sampleName.startsWith('GL-');
  }

  function getIdFromSampleName(sampleName) {
    return sampleName.slice(3);
  }

  function oneToZeroBase(index) {
    return index - 1;
  }

  function newNode(id) {
    return { id: id };
  }

  function createMarriageNode(spouseA, spouseB) {
    var marriageNode = {};
    marriageNode.x = halfwayBetween(spouseA, spouseB);
    marriageNode.y = spouseA.y;
    marriageNode.type = "marriage";
    marriageNode.dataNode = {};
    return marriageNode;
  }

  function createMarriageLink(spouseNode, marriageNode) {
    var marriageLink = {};
    marriageLink.type = 'spouse';
    marriageLink.source = spouseNode;
    marriageLink.target = marriageNode;
    return marriageLink;
  }

  function createChildLink(childNode, marriageNode) {
    var childLink = {};
    childLink.type = 'child';
    childLink.source = childNode;
    childLink.target = marriageNode;
    return childLink;
  }

  function getAllKids(data, nodes, nodeA, nodeB) {
    var father;
    var mother;
    if (nodeA.dataNode.sex === 'male') {
      father = nodeA;
      mother = nodeB;
    }
    else {
      father = nodeB;
      mother = nodeA;
    }

    var kids = [];
    for (var node of nodes) {
      if (node.type != 'marriage') {
        if (data.pedigree.findex[oneToZeroBase(node.dataNode.id)] ===
              father.dataNode.id &&
            data.pedigree.mindex[oneToZeroBase(node.dataNode.id)] ===
              mother.dataNode.id) {
          kids.push(node);
        }
      }
    }

    return kids;
  }

  function halfwayBetween(nodeA, nodeB) {
    if (nodeB.x > nodeA.x) {
      return nodeA.x + ((nodeB.x - nodeA.x) / 2);
    }
    else {
      return nodeB.x + ((nodeA.x - nodeB.x) / 2);
    }
  }

  function halfwayBetweenGeneric(a, b) {
    if (a > b) {
      return a + ((b - a) / 2);
    }
    else {
      return b + ((a - b) / 2);
    }
  }

  //function updatePedigreeFile() {
  //  updateFile('pedigree_file_input', function(fileData) {
  //    pedigreeFileText = fileData;
  //    serverPedigreeAndLayout(function(nodes, links) {
  //      doVisuals(nodes, links);
  //    });
  //  });
  //}

  //function updateDNGOutputFile() {
  //  updateFile('dng_output_file_input', function(fileData) {
  //    serverPedigreeAndLayout(function(nodes, links) {
  //      dngOutputFileText = fileData;
  //      dngOverlay();
  //      doVisuals(nodes, links);
  //    });
  //  });
  //}

  //function updateFile(fileInputElementId, callback) {
  //  var selectedFile = document.getElementById(fileInputElementId).files[0];

  //  if (selectedFile !== undefined) {
  //    var reader = new FileReader();

  //    reader.onload = function(readerEvent) {
  //      callback(reader.result);
  //    };
  //    reader.readAsText(selectedFile);
  //  }
  //}
}

},{"./dng_vcf_parser":6,"./ped_parser":8,"./pedigr":9}],8:[function(require,module,exports){
(function() {
  "use strict";

  function parsePedigreeFile(fileText) {

    var newickParser = require('biojs-io-newick');

    var entries = [];

    var lines = fileText.split('\n');
    for (var line of lines) {

      if (line.length === 0) {
        continue;
      }

      var row = line.split("\t");
      var entry = {};
      entry.familyId = Number(row[0]);
      entry.individualId = Number(row[1]);

      var fatherId = Number(row[2]);
      if (fatherId === 0) {
        entry.fatherId = undefined;
      }
      else {
        entry.fatherId = fatherId;
      }

      var motherId = Number(row[3]);
      if (motherId === 0) {
        entry.motherId = undefined;
      }
      else {
        entry.motherId = motherId;
      }

      var sex = Number(row[4]);
      if (sex === 1) {
        entry.sex = 'male';
      }
      else if (sex === 2) {
        entry.sex = 'female';
      }
      else {
        entry.sex = undefined;
      }

      var sampleIds = row[5];

      if (sampleIds !== undefined) {
        entry.sampleIds =
          newickParser.parse_newick(convertToValidNewick(sampleIds));
      }
      else {
        entry.sampleIds = {
          children: [],
          name: String(entry.individualId)
        };
      }

      entries.push(entry);
    }

    return entries;
  }

  function convertToValidNewick(row) {
    return "(" + row + ")";
  }

  module.exports.parsePedigreeFile = parsePedigreeFile;
}());

},{"biojs-io-newick":2}],9:[function(require,module,exports){
(function() {
  "use strict";

  function PedigreeGraph() {
    this.persons = {};
    this.marriages = [];
    this.parentageLinks = {};
  };

  PedigreeGraph.prototype.getPerson = function(id) {
    return this.persons[id];
  };

  PedigreeGraph.prototype.getPersons = function(query) {
    var persons = [];
    for (var key in this.persons) {
      persons.push(this.persons[key]);
    }
    return persons;
  };

  PedigreeGraph.prototype.addPerson = function(person) {
    this.persons[person.id] = person;
    return person;
  };

  PedigreeGraph.prototype.addMarriage = function(marriage) {
    this.marriages.push(marriage);
    return marriage;
  };

  PedigreeGraph.createGraph = function() {
    var graph = new PedigreeGraph();
    return graph;
  };

  function Person(attr) {
    this.id = attr.id;
    this.sex = attr.sex;
    //this.father = attr.father;
    //this.mother = attr.mother;
    this.parentageLink = attr.parentageLink;
    this.marriageLink = attr.marriageLink;
    this.data = attr.data;
  };

  Person.createPerson = function(attr) {
    return new Person(attr);
  };

  Person.prototype.getParentageLink = function() {
    return this.parentageLink;
  };

  function PersonBuilder(id) {
    this._id = id;
    this._sex = undefined;
    //this._father = undefined;
    //this._mother = undefined;
    this._parentageLink = undefined;
    this._marriageLink = undefined;
    this._data = undefined;
    return this;
  };

  PersonBuilder.prototype.sex = function(sex) {
    this._sex = sex;
    return this;
  }

  PersonBuilder.prototype.parentageLink = function(parentageLink) {
    this._parentageLink = parentageLink;
    return this;
  };

  //PersonBuilder.prototype.father = function(father) {
  //  this._father = father;
  //  return this;
  //};

  //PersonBuilder.prototype.mother = function(mother) {
  //  this._mother = mother;
  //  return this;
  //};

  PersonBuilder.prototype.marriage = function(marriage) {
    this._marriage = marriage;
    return this;
  };

  PersonBuilder.prototype.data = function(data) {
    this._data = data;
    return this;
  };

  PersonBuilder.prototype.build = function() {
    return Person.createPerson({
      id: this._id,
      sex: this._sex,
      //father: this._father,
      //mother: this._mother,
      parentageLink: this._parentageLink,
      marriageLink: this._marriageLink,
      data: this._data
    });
  };

  PersonBuilder.createPersonBuilder = function(id) {
    return new PersonBuilder(id);
  };

  function Marriage(attr) {
    this.fatherLink = attr.fatherLink;
    this.motherLink = attr.motherLink;
    this.childLinks = attr.childLinks;
    this.data = attr.data;
  }

  Marriage.prototype.addSpouse = function(spouse) {
    if (spouse.sex === 'male') {
      var link = MarriageLink.createMarriageLink(this, spouse);
      this.fatherLink = link;
      spouse.marriageLink = link;
    }
    else {
      var link = MarriageLink.createMarriageLink(this, spouse);
      this.motherLink =  link;
      spouse.marriageLink = link;
    }
  };

  Marriage.prototype.addChild = function(child) {
    var parentageLink = ParentageLink.createParentageLink(this, child);
    this.childLinks.push(parentageLink);
    child.parentageLink = parentageLink;
    return parentageLink;
  };

  Marriage.createMarriage = function(attr) {
    return new Marriage(attr);
  };

  function MarriageBuilder() {
    this._father = undefined;
    this._mother = undefined;
    this._children = undefined;
    this._data = undefined;
  }

  MarriageBuilder.createMarriageBuilder = function() {
    return new MarriageBuilder();
  };

  MarriageBuilder.prototype.spouse = function(spouse) {
    if (spouse.sex === 'male') {
      this._father = spouse;
    }
    else {
      this._mother = spouse;
    }
    return this;
  };

  MarriageBuilder.prototype.father = function(father) {
    this._fatherLink = father;
    return this;
  };

  MarriageBuilder.prototype.mother = function(mother) {
    this._mother = mother;
    return this;
  };

  MarriageBuilder.prototype.children = function(children) {
    this._children = children;
    return this;
  };

  MarriageBuilder.prototype.data = function(data) {
    this._data = data;
    return this;
  };

  MarriageBuilder.prototype.build = function() {
    var marriage = new Marriage.createMarriage({
      data: this._data
    });

    marriage.addSpouse(this._father);
    marriage.addSpouse(this._mother);
    marriage.childLinks = [];
    return marriage;
  }

  function MarriageLink(marriage, spouse) {
    this.marriage = marriage;
    this.spouse = spouse;
  }

  MarriageLink.createMarriageLink = function(marriage, spouse) {
    return new MarriageLink(marriage, spouse);
  };

  function ParentageLink(parentage, child) {
    this.parentage = parentage;
    this.child = child;
    this.data = undefined;
  }

  ParentageLink.createParentageLink = function(parentage, child) {
    return new ParentageLink(parentage, child);
  };

  ParentageLink.prototype.setData = function(data) {
    this.data = data;
  };


  module.exports = {
    PedigreeGraph: PedigreeGraph,
    PersonBuilder: PersonBuilder,
    MarriageBuilder: MarriageBuilder
  };

}());

},{}]},{},[6,7,9,8]);
