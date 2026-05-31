#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "device/phh/treble/cmds/persistent_properties.pb.h"
#include <algorithm>
#include <iostream>

int main(int argc, char **argv) {
	int fd = open("persistent_properties", O_RDWR);
	off_t size = lseek(fd, 0, SEEK_END);
	lseek(fd, 0, SEEK_SET);
	char *data = (char*) malloc(size);
	int ret = read(fd, data, size);

	PersistentProperties props;
	bool parsed = props.ParseFromArray(data, size);
	std::cout << "Currently has " << props.properties_size() << " props." << std::endl;
	for(auto prop: props.properties()) {
		std::cout << prop.name() << ":" << prop.value() << std::endl;
	}

	if(argc == 1) {
		close(fd);
		return 0;
	}
	if(argc != 3) {
		std::cout << "Usage: " << argv[0] << " [prop value]" << std::endl;
		return -1;
	}

	std::string property(argv[1]);
	std::string value(argv[2]);

	auto p = props.mutable_properties();
	auto it = std::find_if(p->begin(), p->end(), [=](const auto& v) { return v.name() == property; });
	if(it == p->end()) {
		std::cout << "Property not found, adding it" << std::endl;
		auto *record = p->Add();;
		record->set_name(property);
		record->set_value(value);
	} else {
		std::cout << "Property found, replacing it" << std::endl;
		it->set_value(value);
	}

	size_t write_size = props.ByteSize();
	char *write_buffer = (char*) malloc(write_size);
	props.SerializeToArray(write_buffer, write_size);
	ftruncate(fd, 0);
	lseek(fd, 0, SEEK_SET);
	write(fd, write_buffer, write_size);
	close(fd);
}
