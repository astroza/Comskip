var comskip = require('../build/Release/comskip');

comskip.run(process.argv[2], process.argv[3], process.argv[4], process.argv[5], function(result) {
  process.send({'type': 'finish', 'finish': {'result': result}});
}, function(start, end, completed) {
  process.send({'type': 'update', 'segment': {'start': start, 'end': end, 'completed': completed}});
});
