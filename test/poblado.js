'use strict'

const test = require('tape')
const exec = require('child_process').exec
const path = require('path')
const capitalize = path.join(__dirname, '..', 'bin', 'test_capitalize')

/* eslint-disable comma-spacing */
const payloads = [
    [ 'emptyObject' , { }, [ ] ]
  , [ 'emptyArray'  , [ ], [ ] ]
  , [ 'numberKey'   , { number: 1 }, [ 'NUMBER' ] ]
  , [ 'stringKey'   , { string: 'hello world' }, [ 'STRING', 'HELLO WORLD' ] ]
  , [ 'objectKey'   , { object: { } }, [ 'OBJECT' ] ]
  , [ 'nestedObject'
    , { outer: { inner: { string: 'hola' } } }
    , [ 'OUTER', 'INNER', 'STRING', 'HOLA' ]
    ]
]
/* eslint-enable comma-spacing */

test('\ncapitalizing small JSON payloads', function(t) {
  payloads.forEach(runTest.bind(null, t, 64, 20))
  payloads.forEach(runTest.bind(null, t, 640, 20))
  payloads.forEach(runTest.bind(null, t, 64, 200))
  payloads.forEach(runTest.bind(null, t, 8, 0))
  t.end()
})

function runTest(t, chunkSize, processorDelay, payload) {
  const name = payload[0]
  const obj = payload[1]
  const expected = payload[2]

  test(` ------------- \n## [ chunkSize: ${chunkSize}, processorDelay: ${processorDelay}, payload: ${name} ]`, function(t) {
    const json = JSON.stringify(obj)
    const cmd = `echo '${json}' | ${capitalize} ${chunkSize} ${processorDelay}`
    console.log(cmd)
    exec(cmd, onresult)

    function onresult(error, stdout, stderr) {
      t.iferror(error, 'should not error')
      t.equal(stderr.toString(), 'OK', 'finishes and prints OK')
      const res = stdout.trim().split(',').slice(0, -1)
      t.deepEqual(res, expected, 'extracts strings and capitalizes them')
      t.end()
    }
  })
}
