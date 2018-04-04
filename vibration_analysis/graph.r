library(ggplot2)
library(reshape)

theme_Publication <- function(use_major_gridlines = TRUE, use_minor_gridlines = FALSE, base_size=14, base_family="helvetica") {
      library(grid)
      library(ggthemes)

      major_elem <- element_blank()
      if (use_major_gridlines) {
      	major_elem <- element_line(colour="#f0f0f0")
      }

      minor_elem <- element_blank()
      if (use_minor_gridlines) {
      	minor_elem <- element_line(colour="#f0f0f0")
      }

      (theme_foundation(base_size=base_size, base_family=base_family)
       + theme(plot.title = element_text(face = "bold",
                                         size = rel(1.2), hjust = 0.5),
               text = element_text(),
               panel.background = element_rect(colour = NA),
               plot.background = element_rect(colour = NA),
               panel.border = element_rect(colour = NA),
               axis.title = element_text(face = "bold",size = rel(2)),
               axis.title.y = element_text(angle=90,vjust =2),
               axis.title.x = element_text(vjust = -0.2),
               axis.text = element_text(size = rel(1.5)), 
               axis.line = element_line(colour="black"),
               axis.ticks = element_line(),
               panel.grid.major = major_elem,
               panel.grid.minor = minor_elem,
               legend.key = element_rect(colour = NA),
               legend.position = "bottom",
               legend.direction = "horizontal",
               legend.key.size= unit(0.2, "cm"),
               legend.spacing = unit(0, "cm"),
               legend.title = element_text(face="italic"),
               legend.margin = margin(t = 0, unit = "cm"),
               plot.margin = unit(c(5,5,5,5),"mm"),
               strip.background=element_rect(colour="#f0f0f0",fill="#f0f0f0"),
               strip.text = element_text(face="bold")
          ))
}

scale_fill_Publication <- function(...){
      library(scales)
      discrete_scale("fill","Publication",manual_pal(values = c("#386cb0","#fdb462","#7fc97f","#ef3b2c","#662506","#a6cee3","#fb9a99","#984ea3","#ffff33")), ...)

}

scale_colour_Publication <- function(...){
      library(scales)
      discrete_scale("colour","Publication",manual_pal(values = c("#386cb0","#fdb462","#7fc97f","#ef3b2c","#662506","#a6cee3","#fb9a99","#984ea3","#ffff33")), ...)
}

make_line_plot <- function(data, scale_factor = 1.0, data2 = data.frame(), legend_position = "top",  legend_direction = "horizontal", labels = c("X,", "Y,", "Z")) {
	# Takes the multi-column data into the correct format
	data <- melt(data, id = c("time"))

	# Scales data as requested
	data$value <- data$value * scale_factor

	# Plots the data as lines
	plt <- ggplot(data = data)
	plt <- plt + geom_line(aes(x = data$time, y = value, color = variable), alpha = 0.75, size = 2.0)

	if (length(data2) > 0) {
		data2 <- melt(data2, id = c("time"))

		data2$value <- data2$value * scale_factor

		plt <- plt + geom_line(data = data2, aes(x = data2$time, y = value, color = variable), alpha = 0.75, size = 2.0)
	}

	# Applies the theme and color scale
	plt <- plt + theme_Publication() + scale_colour_Publication(labels = labels)

	# Adjusts the legend
	plt <- plt + theme(legend.position = legend_position, legend.direction = legend_direction, legend.key.size = unit(1, "cm"),
						legend.title = element_text(face = "italic", size = rel(2)),
						legend.text = element_text(size = rel(2)),
						legend.background = element_rect(fill = alpha('white', 0.0)))

	# Overrides the sizes of the lines in the legend and the opacity
	plt <- plt + guides(colour = guide_legend(override.aes = list(size = 3, alpha = 1.0)))	
}

def_height <- 5
def_width <- 7

make_linear_displacement_plot <- function(data, f_name = "acc_displacement") {
	plt <- make_line_plot(data, 1000.0)


	plt <- plt + labs(x = "Time (s)", y = "Linear Displacement (mm)", colour = "Dimension:")	
	ggsave(paste(c("graphs/", f_name, ".png"), collapse = ""), plot = plt, width = def_width, height = def_height)
}

make_angular_displacement_plot <- function(data, f_name = "gyro_angular_displacement") {
   data <- data[c("time", "Roll", "Pitch", "Yaw")]

	plt <- make_line_plot(data, 1000.0)

	plt <- plt + labs(x = "Time (s)", y = "Angular Displacement (mrad)", colour = "Axis of Rotation:")	
	ggsave(paste(c("graphs/", f_name, ".png"), collapse = ""), plot = plt, width = def_width, height = def_height)
}

make_angular_speed_plot <- function(data, f_name = "gyro_speed") {
   data <- data[c("time", "Roll", "Pitch", "Yaw")]

	plt <- make_line_plot(data, 1000.0)

	plt <- plt + labs(x = "Time (s)", y = "Angular Speed (mrad/s)", colour = "Axis of Rotation:")	
	ggsave(paste(c("graphs/", f_name, ".png"), collapse = ""), plot = plt, width = def_width, height = def_height)
}

make_comp_plot <- function(ls_data, acc_data, f_name = "acc_vs_lds") {
	plt <- make_line_plot(ls_data, scale_factor = 1000.0, data2 = acc_data, legend_direction = "vertical", legend_position = "right")

	plt <- plt + labs(x = "Time (s)", y = "Displacement (mm)", colour = "Device:")
	ggsave(paste(c("graphs/", f_name, ".png"), collapse = ""), width = def_width, height = 4) 
}

# Data from the top of racks
linear_displacement_data <- read.table("data/linear.tsv", header = TRUE, sep = "\t")
# angular_displacement_data <- read.table("data/angular_displacement.tsv", header = TRUE, sep = "\t")
# angular_speed_data <- read.table("data/angular_speed.tsv", header = TRUE, sep = "\t")

make_linear_displacement_plot(linear_displacement_data)
# make_angular_displacement_plot(angular_displacement_data)
# make_angular_speed_plot(angular_speed_data)

# Data 
comp_data <- read.table("data/laser_sensor_data.tsv", header = TRUE, sep = "\t")

ls_data <- comp_data[,c("time_ls", "ls_displacement")]
colnames(ls_data) <- c("time", "LDS")

acc_data <- comp_data[,c("time_acc", "acc_displacement")]
colnames(acc_data) <- c("time", "Acc")

acc_data <- subset(acc_data, !is.na(time))

make_comp_plot(ls_data, acc_data)

# Max Speed
# ms_ad_data <- read.table("data/max_speed-angular_displacement.tsv", header = TRUE, sep = "\t")
ms_as_data <- read.table("data/max_speed-angular_speed.tsv", header = TRUE, sep = "\t")

# Max Angle
ma_ad_data <- read.table("data/max_angle-angular_displacement.tsv", header = TRUE, sep = "\t")
# ma_as_data <- read.table("data/max_angle-angular_speed.tsv", header = TRUE, sep = "\t")

# ms_ad_data <- subset(ms_ad_data, !is.na(Pitch))
ms_as_data <- subset(ms_as_data, !is.na(Pitch))
ma_ad_data <- subset(ma_ad_data, !is.na(Pitch))
# ma_as_data <- subset(ma_as_data, !is.na(Pitch))

# make_angular_displacement_plot(ms_ad_data, f_name = "max_speed-angular_displacement")
make_angular_speed_plot(ms_as_data, f_name = "max_speed-angular_speed")
make_angular_displacement_plot(ma_ad_data, f_name = "max_angle-angular_displacement")
# make_angular_speed_plot(ma_as_data, f_name = "max_angle-angular_speed")
