import subprocess as sp

filename = "./src/webdriver/webdriver_version.cc"
p = sp.Popen(["git", "describe" , "--abbrev=10", "--dirty", "--always"], stdin=sp.PIPE, stdout=sp.PIPE)

data = p.stdout.readline()

versionfile = open (filename, 'w')
versionfile.write("namespace webdriver {\n")
versionfile.write("extern const char kProductName[] = \"WebDriver-cisco-cmt\";\n")
versionfile.write("extern const char kVersionNumber[] = \"1.3.3\";\n")
versionfile.write("extern const char kBuildTime[] = __TIME__;\n")
versionfile.write("extern const char kBuildDate[] = __DATE__;\n")
versionfile.write("extern const char kLastChanges[] = \"" + data.strip().decode('utf-8') + "\";\n")
versionfile.write("}")
versionfile.close()

p.stdout.close()
p.stdin.close()
