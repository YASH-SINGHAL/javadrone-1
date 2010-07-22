/**********************************
 Demo Program for HIDAPI
 
 Alan Ott
 Signal 11 Software
 2010-07-20
**********************************/


#include <fx.h>

#include "hidapi.h"
#include <string.h>
#include <stdlib.h>



class MainWindow : public FXMainWindow {
	FXDECLARE(MainWindow)
	
public:
	enum {
		ID_FIRST = FXMainWindow::ID_LAST,
		ID_CONNECT,
		ID_DISCONNECT,
		ID_SEND_OUTPUT_REPORT,
		ID_SEND_FEATURE_REPORT,
		ID_CLEAR,
		ID_TIMER,
		ID_LAST,
	};
	
private:
	FXList *device_list;
	FXButton *connect_button;
	FXButton *disconnect_button;
	FXButton *output_button;
	FXLabel *connected_label;
	FXTextField *output_text;
	FXButton *feature_button;
	FXTextField *feature_text;
	FXText *input_text;
	
	struct hid_device *devices;
	int connected_device;

protected:
	MainWindow() {};
public:
	MainWindow(FXApp *a);
	~MainWindow();
	virtual void create();
	
	long onConnect(FXObject *sender, FXSelector sel, void *ptr);
	long onDisconnect(FXObject *sender, FXSelector sel, void *ptr);
	long onSendOutputReport(FXObject *sender, FXSelector sel, void *ptr);
	long onSendFeatureReport(FXObject *sender, FXSelector sel, void *ptr);
	long onClear(FXObject *sender, FXSelector sel, void *ptr);
	long onTimeout(FXObject *sender, FXSelector sel, void *ptr);
};

// FOX 1.7 changes the timeouts to all be nanoseconds.
// Fox 1.6 had all timeouts as milliseconds.
#if (FOX_MINOR >= 7)
	const int timeout_scalar = 1000*1000;
#else
	const int timeout_scalar = 1;
#endif

FXDEFMAP(MainWindow) MainWindowMap [] = {
	FXMAPFUNC(SEL_COMMAND, MainWindow::ID_CONNECT, MainWindow::onConnect ),
	FXMAPFUNC(SEL_COMMAND, MainWindow::ID_DISCONNECT, MainWindow::onDisconnect ),
	FXMAPFUNC(SEL_COMMAND, MainWindow::ID_SEND_OUTPUT_REPORT, MainWindow::onSendOutputReport ),
	FXMAPFUNC(SEL_COMMAND, MainWindow::ID_SEND_FEATURE_REPORT, MainWindow::onSendFeatureReport ),
	FXMAPFUNC(SEL_COMMAND, MainWindow::ID_CLEAR, MainWindow::onClear ),
	FXMAPFUNC(SEL_TIMEOUT, MainWindow::ID_TIMER, MainWindow::onTimeout ),
};

FXIMPLEMENT(MainWindow, FXMainWindow, MainWindowMap, ARRAYNUMBER(MainWindowMap));

MainWindow::MainWindow(FXApp *app)
	: FXMainWindow(app, "HIDAPI Test Application", NULL, NULL, DECOR_ALL, 200,100, 425,600)
{
	devices = NULL;
	connected_device = -1;

	FXVerticalFrame *vf = new FXVerticalFrame(this, LAYOUT_FILL_Y|LAYOUT_FILL_X);

	FXLabel *label = new FXLabel(vf, "HIDAPI Test Tool");
	label->setFont(new FXFont(getApp(), "Arial", 14, FXFont::Bold));
	
	new FXLabel(vf,
		"Select a device and press Connect.", NULL, JUSTIFY_LEFT);
	new FXLabel(vf,
		"Output data bytes can be entered in the Output section, \n"
		"separated by space, comma or brackets. Data starting with 0x\n"
		"is treated as hex. Data beginning with a 0 is treated as \n"
		"octal. All other data is treated as decimal.", NULL, JUSTIFY_LEFT);
	new FXLabel(vf,
		"Data received from the device appears in the Input section.",
		NULL, JUSTIFY_LEFT);
	new FXLabel(vf, "");

	// Device List and Connect/Disconnect buttons
	FXHorizontalFrame *hf = new FXHorizontalFrame(vf, LAYOUT_FILL_X);
	//device_list = new FXList(new FXHorizontalFrame(hf,FRAME_SUNKEN|FRAME_THICK, 0,0,0,0, 0,0,0,0), NULL, 0, LISTBOX_NORMAL|LAYOUT_FILL_X|LAYOUT_FILL_Y|LAYOUT_FIX_WIDTH|LAYOUT_FIX_HEIGHT, 0,0,300,200);
	device_list = new FXList(new FXHorizontalFrame(hf,FRAME_SUNKEN|FRAME_THICK|LAYOUT_FILL_X|LAYOUT_FILL_Y, 0,0,0,0, 0,0,0,0), NULL, 0, LISTBOX_NORMAL|LAYOUT_FILL_X|LAYOUT_FILL_Y, 0,0,300,200);
	FXVerticalFrame *buttonVF = new FXVerticalFrame(hf);
	connect_button = new FXButton(buttonVF, "Connect", NULL, this, ID_CONNECT, BUTTON_NORMAL|LAYOUT_FILL_X);
	disconnect_button = new FXButton(buttonVF, "Disconnect", NULL, this, ID_DISCONNECT, BUTTON_NORMAL|LAYOUT_FILL_X);
	new FXHorizontalFrame(buttonVF, 0, 0,0,0,0, 0,0,50,0);

	connected_label = new FXLabel(vf, "Disconnected");
	
	new FXHorizontalFrame(vf);
	
	// Output Group Box
	FXGroupBox *gb = new FXGroupBox(vf, "Output", FRAME_GROOVE|LAYOUT_FILL_X);
	FXMatrix *matrix = new FXMatrix(gb, 2, MATRIX_BY_COLUMNS|LAYOUT_FILL_X);
	//hf = new FXHorizontalFrame(gb, LAYOUT_FILL_X);
	output_text = new FXTextField(matrix, 40, NULL, 0, TEXTFIELD_NORMAL|LAYOUT_FILL_X|LAYOUT_FILL_COLUMN);
	output_text->setText("1 0x81 0");
	output_button = new FXButton(matrix, "Send Output Report", NULL, this, ID_SEND_OUTPUT_REPORT, BUTTON_NORMAL|LAYOUT_FILL_X);
	output_button->disable();
	//new FXHorizontalFrame(matrix, LAYOUT_FILL_X);

	//hf = new FXHorizontalFrame(gb, LAYOUT_FILL_X);
	feature_text = new FXTextField(matrix, 40, NULL, 0, TEXTFIELD_NORMAL|LAYOUT_FILL_X|LAYOUT_FILL_COLUMN);
	feature_button = new FXButton(matrix, "Send Feature Report", NULL, this, ID_SEND_FEATURE_REPORT, BUTTON_NORMAL|LAYOUT_FILL_X);
	feature_button->disable();


	// Input Group Box
	gb = new FXGroupBox(vf, "Input", FRAME_GROOVE|LAYOUT_FILL_X|LAYOUT_FILL_Y);
	FXVerticalFrame *innerVF = new FXVerticalFrame(gb, LAYOUT_FILL_X|LAYOUT_FILL_Y);
	input_text = new FXText(new FXHorizontalFrame(innerVF,LAYOUT_FILL_X|LAYOUT_FILL_Y|FRAME_SUNKEN|FRAME_THICK, 0,0,0,0, 0,0,0,0), NULL, 0, LAYOUT_FILL_X|LAYOUT_FILL_Y);
	input_text->setEditable(false);
	new FXButton(innerVF, "Clear", NULL, this, ID_CLEAR, BUTTON_NORMAL|LAYOUT_RIGHT);
	

}

MainWindow::~MainWindow()
{

}

void
MainWindow::create()
{
	FXMainWindow::create();
	show();
	
	struct hid_device *cur_dev;
	
	// List the Devices
	hid_free_enumeration(devices);
	devices = hid_enumerate(0x0, 0x0);
	cur_dev = devices;	
	while (cur_dev) {
		printf("Device Found\n  type: %04hx %04hx\n  path: %s\n  serial_number: %ls", cur_dev->vendor_id, cur_dev->product_id, cur_dev->path, cur_dev->serial_number);
		printf("\n");
		printf("  Manufacturer: %ls\n", cur_dev->manufacturer_string);
		printf("  Product:      %ls\n", cur_dev->product_string);
		printf("\n");
		
		// Add it to the List Box.
		FXString s;
		s.format("%04hx:%04hx -", cur_dev->vendor_id, cur_dev->product_id);
		s += FXString(" ") + cur_dev->manufacturer_string;
		s += FXString(" ") + cur_dev->product_string;
		FXListItem *li = new FXListItem(s, NULL, cur_dev);
		device_list->appendItem(li);
		
		cur_dev = cur_dev->next;
	}

	if (device_list->getNumItems() == 0)
		device_list->appendItem("*** No Devices Connected ***");
	else {
		device_list->selectItem(0);
	}

}

long
MainWindow::onConnect(FXObject *sender, FXSelector sel, void *ptr)
{
	if (connected_device != -1)
		return 1;
	
	FXint cur_item = device_list->getCurrentItem();
	if (cur_item < 0)
		return -1;
	FXListItem *item = device_list->getItem(cur_item);
	if (!item)
		return -1;
	struct hid_device *device_info = (struct hid_device*) item->getData();
	if (!device_info)
		return -1;
	
	connected_device =  hid_open_path(device_info->path);
	
	if (connected_device < 0) {
		FXMessageBox::error(this, MBOX_OK, "Device Error", "Unable To Connect to Device");
		return -1;
	}
	
	hid_set_nonblocking(connected_device, 1);

	getApp()->addTimeout(this, ID_TIMER,
		5 * timeout_scalar /*5ms*/);
	
	FXString s;
	s.format("Connected to: %04hx:%04hx -", device_info->vendor_id, device_info->product_id);
	s += FXString(" ") + device_info->manufacturer_string;
	s += FXString(" ") + device_info->product_string;
	connected_label->setText(s);
	output_button->enable();
	feature_button->enable();
	input_text->setText("");


	return 1;
}

long
MainWindow::onDisconnect(FXObject *sender, FXSelector sel, void *ptr)
{
	hid_close(connected_device);
	connected_device = -1;
	connected_label->setText("Disconnected");
	output_button->disable();
	feature_button->disable();

	getApp()->removeTimeout(this, ID_TIMER);
	
	return 1;
}

long
MainWindow::onSendOutputReport(FXObject *sender, FXSelector sel, void *ptr)
{
	const char *delim = " ,{}\t\r\n";
	FXString data = output_text->getText();
	const FXchar *d  = data.text();
	char buf[256];
	int i = 0;
	
	// Copy the string from the GUI.
	size_t sz = strlen(d);
	char *str = (char*) malloc(sz+1);
	strcpy(str, d);
	
	// For each token in the string, parse and store in buf[].
	char *token = strtok(str, delim);
	while (token) {
		char *endptr;
		long int val = strtol(token, &endptr, 0);
		buf[i++] = val;
		printf("%02hhx\n", (char) buf[i-1]);
		token = strtok(NULL, delim);
	}
	
	int res = hid_write(connected_device, (const unsigned char*)buf, i);
	if (res < 0) {
		FXMessageBox::error(this, MBOX_OK, "Error Writing", "Could not write to device. Error reported was %s", hid_error(connected_device));
	}
	
	return 1;
}

long
MainWindow::onSendFeatureReport(FXObject *sender, FXSelector sel, void *ptr)
{
	return 1;
}

long
MainWindow::onClear(FXObject *sender, FXSelector sel, void *ptr)
{
	input_text->setText("");
	return 1;
}

long
MainWindow::onTimeout(FXObject *sender, FXSelector sel, void *ptr)
{
	unsigned char buf[256];
	int res = hid_read(connected_device, buf, sizeof(buf));
	
	if (res > 0) {
		FXString s;
		s.format("Received %d bytes:\n", res);
		for (int i = 0; i < res; i++) {
			FXString t;
			t.format("%02hhx ", buf[i]);
			s += t;
			if ((i+1) % 4 == 0)
				s += " ";
			if ((i+1) % 16 == 0)
				s += "\n";
		}
		s += "\n";
		input_text->appendText(s);
	}

	getApp()->addTimeout(this, ID_TIMER,
		5 * timeout_scalar /*5ms*/);
	return 1;
}

int main(int argc, char **argv)
{
	FXApp app("HIDAPI Test Application", "Signal 11 Software");
	app.init(argc, argv);
	new MainWindow(&app);
	app.create();
	app.run();
	return 0;
}
