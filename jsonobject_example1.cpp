#include <iostream>
#include "jsonobject.h"


int main(int argc, const char** argv)
{
    (void)argc;
    (void)argv;

    JSONobject json("toptag");
    JSONobject& ref = json.add("innertag");
    ref.add("child1", "sammy");
    ref.add("child3", "frank"); // Retains inserted order
    ref.add("child2", "tootie");

    // Print third child of inner object
    std::cout << json["innertag"][2].value() << std::endl;

    // Print tab formatted JSON output
    std::cout << std::endl << json.getJSONFormatted();

    // Print tab formatted XML output
    std::cout << std::endl << json.getXMLFormatted();
}
