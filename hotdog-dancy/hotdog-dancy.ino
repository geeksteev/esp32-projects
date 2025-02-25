#include "esp_camera.h"
#include "img_converters.h"
#include "Arduino.h"
#include "fb_gfx.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "dl_lib_matrix3d.h"
#include <SPI.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/version.h"
#include "model.h" // Include the quantized TFLite model as a header

// Camera pins for ESP32-CAM
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// Display connection
TFT_eSPI tft = TFT_eSPI();

// TFLite globals
namespace {
  tflite::ErrorReporter* error_reporter = nullptr;
  const tflite::Model* model = nullptr;
  tflite::MicroInterpreter* interpreter = nullptr;
  TfLiteTensor* input = nullptr;
  
  // Create a memory buffer for the tensor arena
  constexpr int kTensorArenaSize = 200 * 1024;
  uint8_t tensor_arena[kTensorArenaSize];
}

// Class names (top-level taxonomy)
const char* CLASSES[] = {
  "person", "bicycle", "car", "motorcycle", "airplane", 
  "bus", "train", "truck", "boat", "traffic light", 
  "fire hydrant", "stop sign", "parking meter", "bench", "bird", 
  "cat", "dog", "horse", "sheep", "cow", 
  "elephant", "bear", "zebra", "giraffe", "backpack"
  // Add more classes as needed
};

// Full taxonomy structure
struct Taxonomy {
  const char* kingdom;
  const char* phylum;
  const char* class_name;
  const char* order;
  const char* family;
  const char* genus;
  const char* species;
};

// Example taxonomy for some common objects
Taxonomy taxonomies[] = {
  {"Animalia", "Chordata", "Mammalia", "Carnivora", "Felidae", "Felis", "Felis catus"}, // cat
  {"Animalia", "Chordata", "Mammalia", "Carnivora", "Canidae", "Canis", "Canis familiaris"}, // dog
  {"Animalia", "Chordata", "Aves", "Passeriformes", "Varied", "Varied", "Various species"}, // bird
  {"Animalia", "Chordata", "Mammalia", "Proboscidea", "Elephantidae", "Loxodonta/Elephas", "Various species"}, // elephant
  {"Animalia", "Chordata", "Mammalia", "Perissodactyla", "Equidae", "Equus", "Equus quagga"}, // zebra
  {"Artificialia", "Transportation", "Vehicle", "Passenger", "Automobile", "Various", "Various models"}, // car
  {"Animalia", "Chordata", "Mammalia", "Primates", "Hominidae", "Homo", "Homo sapiens"} // person
};

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  
  Serial.begin(115200);
  Serial.println("ESP32-CAM Object Recognition System");
  
  // Initialize the camera
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  // Initialize with higher resolution for better recognition
  config.frame_size = FRAMESIZE_VGA; // 640x480
  config.jpeg_quality = 10;
  config.fb_count = 2;
  
  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
  
  // Initialize display
  tft.begin();
  tft.setRotation(2); // Adjust based on your mounting
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(0, 0);
  tft.println("Object Recognition");
  tft.println("System Initializing...");
  
  // Set up logging
  static tflite::MicroErrorReporter micro_error_reporter;
  error_reporter = &micro_error_reporter;
  
  // Map the model into a usable data structure
  model = tflite::GetModel(model_tflite);
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    TF_LITE_REPORT_ERROR(error_reporter,
                         "Model schema version %d not equal "
                         "to supported version %d.",
                         model->version(), TFLITE_SCHEMA_VERSION);
    return;
  }
  
  // Pull in all operation implementations
  static tflite::AllOpsResolver resolver;
  
  // Build an interpreter to run the model
  static tflite::MicroInterpreter static_interpreter(
      model, resolver, tensor_arena, kTensorArenaSize, error_reporter);
  interpreter = &static_interpreter;
  
  // Allocate memory for the model's input tensors
  TfLiteStatus allocate_status = interpreter->AllocateTensors();
  if (allocate_status != kTfLiteOk) {
    TF_LITE_REPORT_ERROR(error_reporter, "AllocateTensors() failed");
    return;
  }
  
  // Get information about the model's input and output tensors
  input = interpreter->input(0);
  
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 0);
  tft.println("System Ready!");
  delay(1000);
}

void loop() {
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 0);
  tft.println("Capturing image...");
  
  // Get camera frame
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    tft.println("Camera capture failed");
    delay(1000);
    return;
  }
  
  tft.println("Processing image...");
  
  // Process the image for the model (resize to model input dimensions)
  // For example, many models use 224x224 input
  uint8_t resized_image[224 * 224 * 3]; // RGB format
  
  // Resize and preprocess image here (example function)
  process_image(fb->buf, fb->len, resized_image, 224, 224);
  
  // Copy image data to input tensor
  for (int i = 0; i < 224 * 224 * 3; i++) {
    input->data.uint8[i] = resized_image[i];
  }
  
  // Run the model
  TfLiteStatus invoke_status = interpreter->Invoke();
  if (invoke_status != kTfLiteOk) {
    tft.println("Invoke failed");
    delay(1000);
    return;
  }
  
  // Get output tensor
  TfLiteTensor* output = interpreter->output(0);
  
  // Find the class with highest probability
  int highest_pred = 0;
  float highest_score = 0.0;
  
  for (int i = 0; i < 25; i++) { // Assuming 25 classes
    float score = output->data.f[i];
    if (score > highest_score) {
      highest_score = score;
      highest_pred = i;
    }
  }
  
  // Display results if confidence is high enough
  if (highest_score > 0.6) {
    display_results(highest_pred, highest_score);
  } else {
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 0);
    tft.println("No object detected");
    tft.println("with high confidence");
  }
  
  // Release the frame buffer
  esp_camera_fb_return(fb);
  
  // Wait before next capture
  delay(2000);
}

// Function to process/resize image (simplified)
void process_image(const uint8_t* input_data, size_t input_length, uint8_t* output_data, int out_width, int out_height) {
  // In a real application, you would:
  // 1. Decode JPEG image
  // 2. Resize to required dimensions
  // 3. Normalize pixel values (e.g., to [-1,1] or [0,1])
  
  // This is a placeholder - you'll need image processing functions
  // like JpegDec or TJpgDec library for actual implementation
  
  // For testing, fill with dummy data
  for (int i = 0; i < out_width * out_height * 3; i++) {
    output_data[i] = 128; // Default gray value
  }
}

// Find taxonomy for the detected object
int find_taxonomy_index(int class_index) {
  // Map class index to taxonomy index
  // This is a simplified mapping; in a real application,
  // you might have a more sophisticated lookup system
  
  switch(class_index) {
    case 15: return 0; // cat
    case 16: return 1; // dog
    case 14: return 2; // bird
    case 20: return 3; // elephant
    case 22: return 4; // zebra
    case 2:  return 5; // car
    case 0:  return 6; // person
    default: return -1; // unknown
  }
}

// Display results on the TFT screen
void display_results(int class_index, float confidence) {
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 0);
  
  tft.println("Object Detected:");
  tft.setTextSize(3);
  tft.println(CLASSES[class_index]);
  tft.setTextSize(1);
  tft.print("Confidence: ");
  tft.print(confidence * 100);
  tft.println("%");
  tft.println();
  
  // Find and display taxonomy
  int tax_index = find_taxonomy_index(class_index);
  if (tax_index >= 0) {
    tft.setTextSize(2);
    tft.println("Taxonomy:");
    tft.setTextSize(1);
    tft.print("Kingdom: ");
    tft.println(taxonomies[tax_index].kingdom);
    tft.print("Phylum:  ");
    tft.println(taxonomies[tax_index].phylum);
    tft.print("Class:   ");
    tft.println(taxonomies[tax_index].class_name);
    tft.print("Order:   ");
    tft.println(taxonomies[tax_index].order);
    tft.print("Family:  ");
    tft.println(taxonomies[tax_index].family);
    tft.print("Genus:   ");
    tft.println(taxonomies[tax_index].genus);
    tft.print("Species: ");
    tft.println(taxonomies[tax_index].species);
  } else {
    tft.println("Taxonomy not available");
    tft.println("for this object class");
  }
}