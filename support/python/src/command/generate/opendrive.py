from jinja2 import Environment, FileSystemLoader, select_autoescape
import xml.etree.ElementTree as ET
import json
import os
from support.python.src.globals import ESMINI_DIRECTORY_SUPPORT, ESMINI_DIRECTORY_ROOT


class OpenDrive:
    def generate_file(self, data, output):
        """
            Handles the generation of the .json and .hpp files

            Inputs:
            ---------
            data (dict): dictionary used in jinja generation
            output (str): name of file to generate
        """
        outputfolder = os.path.join(ESMINI_DIRECTORY_SUPPORT, "generated")
        if not os.path.exists(outputfolder):
            os.mkdir(outputfolder)
        output_file = os.path.join(outputfolder, output + ".hpp")

        # Dump dict to json for testing
        self.print_dict(output_file, data)

        # Generate the hpp file
        self.create_hpp_files(output_file, data)

    def create_hpp_files(self, output_file, data):
        """
            Generates the .hpp files using jinja2

            Inputs:
            ---------
            output_file (path): the name/location of file to generate
            data (dict): dictionary used in jinja generation
        """
        template_folder = os.path.join(ESMINI_DIRECTORY_SUPPORT, "jinja")
        template_file = "hpp_template.j2"
        env = Environment(
            autoescape=select_autoescape(),
            loader=FileSystemLoader(template_folder),
            trim_blocks=True,
            lstrip_blocks=True,
        )
        template = env.get_template(template_file)
        content = template.render(data)
        with open(output_file, mode="w", encoding="utf-8") as message:
            message.write(content)

    def print_dict(self, output_file, data):
        """
            Generates the .json, primarly for debugging/testing\n
            Dumps the data dictionary to a json file

            Inputs:
            ---------
            output_file (path): the name/location of file to generate
            data (dict): dictionary used in jinja generation
        """
        with open(output_file + ".json", mode="w", encoding="utf-8") as file:
            json.dump(data, file, indent=4)
            file.close()

    def parser(self, file, name):
        """
            Parses a .xsd file in to a dictionary

            Inputs:
            ---------
            file (path): the path to file to be parsed
            name (str): name of file to be generated

            Returns:
            ---------
            dict <- containing the parsed file
        """
        parsed_data = {}
        tree = ET.parse(file)
        root = tree.getroot()
        self.parse_children(root, parsed_data)
        parsed_data = self.union_to_struct(parsed_data)
        return {"name": name, "data": parsed_data}

    def union_to_struct(self,data):
        """
            Restructures a dictionary containing the xsd keyword union to
            fit the structure of struct

            Inputs:
            ---------
            data (dict): dictionary to be fixed

            Returns:
            ---------
            dict <- containing the restructerd dict
        """
        new_dict = {}
        structs = []
        # get all structs
        for item in data.items():
            if "struct" in item[0]:
                structs.append(item)
        # create correct dictionaries for each struct
        struct_members = []
        for key,value in structs:
            members_dict = {}
            for _,value1 in value.items():
                for _,value2 in value1.items():
                    members = value2.split(" ")
                    for member in members:
                        for data_key in data.keys():
                            if member in data_key:
                                struct_members.append(data_key)
                                members_dict.update({data_key:data.get(data_key)})
                        #special case if struct contains values for core file
                        if member == "t_grEqZero" or member == "t_grZero":
                            #Remove struct t_ to get the name
                            member_name = "double "+key[9:]
                            members_dict.update({member_name:""})
            new_dict.update({key:members_dict})
        #copy all other datastructures to the new dict
        #expect the ones that exists in the newly created structs
        for key,value in data.items():
            if key in struct_members or "struct" in key:
                pass
            else:
                new_dict.update({key:value})
        return new_dict

    def parse_children(self, parent, data):
        """
            Parses the xml.tree and extracts necessary data for jinja2 generation

            Inputs:
            ---------
            parent (xml.tree): xml to be parsed
            data (dict): dictionary to append parsed data to

            Returns:
            ---------
            data (dict)
        """
        attributes_dict = {}
        for child in parent:
            if "complexType" in child.tag or "simpleType" in child.tag:
                child_sub_dict = self.parse_children(child, {})
                name = child.attrib["name"]
                if "union" in child_sub_dict.keys():
                    name = "struct " + name
                if "base" in child_sub_dict.keys():
                    sub_sub_dict = child_sub_dict["base"]
                    for key,value in sub_sub_dict.items():
                        if value == {}:
                            name = key + " " + name
                if name.startswith("t_"):
                    name = "class " + name
                elif name.startswith("e_"):
                    name = "enum class " + name
                data.update({name: child_sub_dict})

            elif "extension" in child.tag or "restriction" in child.tag:
                base = child.attrib["base"]
                if base == "xs:string":
                    base = "std::string"
                elif base == "xs:double":
                    base = "double"
                data.update({"base": {base: self.parse_children(child, {})}})

            elif "sequence" in child.tag:
                data.update({"sequence": self.parse_children(child, {})})

            elif "enumeration" in child.tag:
                value = child.attrib["value"]
                value = self.fix_non_legal_chars(value)
                data.update({value: ""})

            elif "element" in child.tag:
                attributes = child.attrib
                if "type" in attributes:
                    attributes["type"] = self.replace_xsd_types(attributes["type"])
                if "maxOccurs" in attributes:
                    if attributes["maxOccurs"] == "unbounded":
                        attributes["type"] = "std::vector<"+attributes["type"]+">"
                data.update({child.attrib["name"]: attributes})

            elif "attribute" in child.tag:
                attributes = child.attrib
                if len(attributes) > 1:
                    attributes["type"] = self.replace_xsd_types(attributes["type"])
                attributes_dict.update({child.attrib["name"]: attributes})
            elif "union" in child.tag:
                data.update({"union":child.attrib})
            else:
                self.parse_children(child, data)
        if len(attributes_dict) != 0:
            data.update({"attributes": attributes_dict})
        return data

    def replace_xsd_types(self, attribute):
        """
            Replaces xsd types with corresponding cpp types

            Inputs:
            ---------
            attribute (str): str containing the type

            Returns:
            ---------
            attribute (str) <- with the corresponding cpp type
        """
        if attribute == "xs:string":
            attribute = "std::string"
        elif attribute == "xs:double":
            attribute = "double"
        elif attribute == "xs:integer":
            attribute = "int"
        elif attribute == "xs:negativeInteger":
            attribute = "int"
        elif attribute == "xs:float":
            attribute = "float"
        elif attribute == "t_grEqZero": # TODO should be fetch or linked to core file
            attribute = "double"
        elif attribute == "t_grZero": # TODO should be fetch or linked to core file
            attribute = "double"
        return attribute

    def fix_non_legal_chars(self, string):
        """
            Remove non legal chars from string

            Inputs:
            ---------
            string (str): string to be checked

            Returns:
            ---------
            string (str) <- without non legal chars
        """
        string = string.replace("/", "")
        string = string.replace("+", "positive") #TODO agree on naming
        string = string.replace("-", "negative") #TODO agree on naming
        string = string.replace("%", "percent") #TODO agree on naming
        return string

    def generate_opendrive(self):
        """
            Generates all opendrive files
        """
        opendrive_schema_path = os.path.join(
            ESMINI_DIRECTORY_ROOT, "..", "OpenDrive1_7_0"
        )
        files_to_generate = [
            (os.path.join(opendrive_schema_path, "opendrive_17_core.xsd"), "Core"),
            (os.path.join(opendrive_schema_path, "opendrive_17_road.xsd"), "Road"),
            (os.path.join(opendrive_schema_path, "opendrive_17_lane.xsd"), "Lane"),
            (
                os.path.join(opendrive_schema_path, "opendrive_17_junction.xsd"),
                "Junction",
            ),
            (os.path.join(opendrive_schema_path, "opendrive_17_object.xsd"), "Object"),
            (os.path.join(opendrive_schema_path, "opendrive_17_signal.xsd"), "Signal"),
            (
                os.path.join(opendrive_schema_path, "opendrive_17_railroad.xsd"),
                "Railroad",
            ),
        ]
        for opendrive, output in files_to_generate:
            print("Parsing: ", output)
            with open(opendrive, mode="r", encoding="utf-8") as input:
                data = self.parser(input, output)
            self.generate_file(data, output)
            print(f"Generated: {output}")
