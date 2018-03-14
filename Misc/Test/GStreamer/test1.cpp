#include <string>
#include <iostream>

#include <stdio.h>
#include <gst/gst.h>

using namespace std;

int main(int argc, char *argv[]) {
   GstElement *pipeline;
   GstBus *bus;
   GstMessage *msg;


   cout << "Parsing command line arguments...\n";
   std::string source_uri;
   if (argc > 2) {
      cout << "Usage: <app> <optional source URI>\n";
      return 1;
   }
   else if (argc == 2) {
      source_uri = argv[1];
   }
   else {
      source_uri = "https://www.freedesktop.org/software/gstreamer-sdk/data/media/sintel_trailer-480p.webm";
   }
   
   cout << "Source URI: " << source_uri << endl;
   
  
   /* Initialize GStreamer */
   cout << "Initializing GStreamer...\n";
   int fake_argc = 1;
   gst_init (&fake_argc, &argv);

   /* Build the pipeline */
   cout << "Building pipeline...\n";
   string pipeline_string = "playbin uri=" + source_uri;
   cout << "Pipeline string: " << pipeline_string << endl;
   pipeline = gst_parse_launch (pipeline_string.c_str(), NULL);

   /* Start playing */
   cout << "Playing...\n";
   gst_element_set_state (pipeline, GST_STATE_PLAYING);

   /* Wait until error or EOS */
   bus = gst_element_get_bus (pipeline);
   msg = gst_bus_timed_pop_filtered (bus, GST_CLOCK_TIME_NONE,
      (GstMessageType) (GST_MESSAGE_ERROR | GST_MESSAGE_EOS));
      
   cout <<"Finished playing.\n";

   /* Free resources */
   if (msg != NULL)
      gst_message_unref (msg);
   gst_object_unref (bus);
   gst_element_set_state (pipeline, GST_STATE_NULL);
   gst_object_unref (pipeline);
   return 0;
}
