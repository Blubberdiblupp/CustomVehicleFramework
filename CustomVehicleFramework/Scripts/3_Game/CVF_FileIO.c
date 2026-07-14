class CVF_FileIO
{
	static const int MAX_TEXT_FILE_CHARS = 32000000;

	static bool EnsureDirectory(string path)
	{
		if (FileExist(path))
			return true;
		return MakeDirectory(path);
	}

	static bool EnsureServerDirectories()
	{
		if (!EnsureDirectory(CVF_Constants.CONFIG_DIR)) return false;
		if (!EnsureDirectory(CVF_Constants.GENERATED_MOD_DIR)) return false;
		return true;
	}

	static bool ReadAllText(string path, out string content)
	{
		content = "";
		if (!FileExist(path))
			return false;

		FileHandle file = OpenFile(path, FileMode.READ);
		if (!file)
			return false;

		int read = ReadFile(file, content, MAX_TEXT_FILE_CHARS);
		CloseFile(file);
		return read >= 0;
	}

	static bool WriteAllText(string path, string content)
	{
		FileHandle file = OpenFile(path, FileMode.WRITE);
		if (!file)
			return false;

		FPrint(file, content);
		CloseFile(file);
		return true;
	}

	static bool WriteAllTextIfChanged(string path, string content)
	{
		string existing;
		if (ReadAllText(path, existing) && existing == content)
			return true;
		return WriteAllText(path, content);
	}

	static bool CopyTextFile(string sourcePath, string destinationPath)
	{
		string content;
		if (!ReadAllText(sourcePath, content))
			return false;
		return WriteAllTextIfChanged(destinationPath, content);
	}
}
