var comskip = require("./build/Release/comskip");

comskip.run("/media/psf/Home/download.mpg", 
	function(status) {
		console.log("termino con status="+status);
	}, 
	function(start, end) {
		console.log("Se encontro comercial en ("+start+", "+end+")");
	}
);
