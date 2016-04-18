var comskip = require('../build/Release/comskip');
var fs = require('fs-extra');
var path = require('path');

function ComskipWrapper(profile, working_dir)
{
  if(profile == null)
    profile = 'default';
  
  if(working_dir== null)
    working_dir = './comskip_data';
  
  this.profile = profile;
  this.working_dir = working_dir;
  
  fs.mkdirsSync(this.output_dir());
}

ComskipWrapper.prototype.ini_file = function() {
  var ini_path = this.working_dir + '/' + this.profile + '/comskip.ini';
  if(!fs.existsSync(ini_path))
    fs.copySync(path.dirname(module.filename) + '/../comskip.ini', ini_path);
  return ini_path;
}

ComskipWrapper.prototype.logo_file = function() {
  return this.working_dir + '/' + this.profile + '/logo.txt';
}

ComskipWrapper.prototype.output_dir = function() {
  return this.working_dir + '/' + this.profile + '/output';
}

ComskipWrapper.prototype.run = function(mpeg_file, finish_cb, update_cb) 
{
  comskip.run(mpeg_file, this.ini_file(), this.logo_file(), this.output_dir(), finish_cb, update_cb);
}

module.exports = ComskipWrapper;
