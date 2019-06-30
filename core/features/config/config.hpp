#include <Windows.h>
#include <string>
#include <vector>

class c_config {
public:
	int counts;
	bool save_config(std::string file_name);
	bool load_config(std::string file_name);
	bool remove_config(std::string file_name);
	void create_config(std::string name);
	std::vector<std::string> get_configs();
	

};

extern c_config config_system;
