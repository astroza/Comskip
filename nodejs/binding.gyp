{
  "targets": [{
    "target_name": "comskip",
    "sources": [ "comskip.cpp" ],
    "conditions": [
      ['OS=="linux"', {
        "link_settings": {
          "libraries": [ "-lcomskip" ]
        }
      }]
    ]
  }]
}
