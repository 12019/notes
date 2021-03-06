#!/usr/bin/env python
import json
import os
from collections import namedtuple
import fnmatch

class DDFileList(object):
	"""
	The object requirs 2 parameters:
	1. The start path to scan the files
	2. The json format of what files should be listed.
	"""
	def __init__(self, start_path, given_json):

		# flist fmt: {"KERN":["kern.info", "kern.info.1"]}
		self.flist = {}
		self.given_json = given_json
		self.start_path = start_path

		self.__process_data__()

	def show(self):
		for key, values in self.flist.items():
			print("%s %s" % (key, values))

	def get_patterns(self):
		return self.flist.keys()

	def get_files(self, pattern):
		return self.flist.get(pattern)

	def find_files(self, directory, pattern):
		for root, dirs, files in os.walk(directory):
			for basename in files:
				if fnmatch.fnmatch(basename, pattern):
					filename = os.path.join(root, basename)
					yield filename

	def json2obj(self, data):
		def _json_object_hook(d):
			return namedtuple('X', d.keys())(*d.values())

		return json.load(data, object_hook=_json_object_hook)

	def __process_data__(self):
		with open(self.given_json) as json_input:
			json_data = self.json2obj(json_input)
			for jdata in json_data:
				# print("jdata: ", jdata)

				# walkthough the pattern list, find the applicable file & add it to dict
				for filename in self.find_files(self.start_path, jdata.list_pattern):
					# print("jdata.name %s fname %s" % (jdata.tag, filename))

					# Instead of doing:
					#    if jdata.tag not in self.flist:
					#       self.flist[jdata.tag] = []
					#    self.flist[jdata.tag].append(filename)
					# This is OneLiner
					self.flist.setdefault(jdata.tag, []).append(filename)
					#if "search_pattern" in jdata.__dict__:
					#	print(jdata.search_pattern)

		# flist_obj.show()
		# print("%s" % json.dumps(flist_obj.__dict__, indent=3))
		# print("%s" % json.dumps(flist_obj, default=convert_to_builtin_type, indent=3))

