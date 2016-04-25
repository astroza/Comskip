var comskip = require('../build/Release/comskip');

comskip.run(process.argv[2], process.argv[3], process.argv[4], process.argv[5], function(result) {
  process.send({'type': 'finish', 'finish': {'result': result}});
}, function(start, end) {
  process.send({'type': 'update', 'update': {'start': start, 'end': end}});
});
