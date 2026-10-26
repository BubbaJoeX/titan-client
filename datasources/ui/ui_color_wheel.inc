<!--
	Color Wheel UI Component
	Uses a color wheel texture for color selection.
	- Click/drag on wheel to select color
	- RGB textbox inputs for fine control
	- HTML hex input
-->
<Page
	Location='100,100'
	MinimumSize='320,340'
	Name='ColorWheel'
	RStyleDefault='rs_default'
	ScrollExtent='320,340'
	Size='320,340'
	UserMovable='true'
	Visible='false'
>
	<Data
		Name='CodeData'
		pageWheel='content.pageWheel'
		cursorWheel='content.pageWheel.cursorWheel'
		pagePreview='htmlRow.pageSample'
		textboxHtml='htmlRow.textboxHtml'
		textR='content.rgbRow.textboxR'
		textG='content.rgbRow.textboxG'
		textB='content.rgbRow.textboxB'
	/>

	<!-- Title Bar -->
	<Page
		BackgroundColor='#213243'
		BackgroundOpacity='0.95'
		Name='titleBar'
		ScrollExtent='320,24'
		Size='320,24'
	>
		<Text
			Font='bold_14'
			LocalText='Color Wheel'
			Location='8,3'
			ScrollExtent='200,18'
			Size='200,18'
			TextColor='#A6CEF0'
		>Color Wheel</Text>
		<Button
			Location='300,4'
			Name='buttonClose'
			ScrollExtent='16,16'
			Size='16,16'
			Style='/Styles.Frame.buttonClose.style'
		/>
	</Page>

	<!-- Main Content -->
	<Page
		BackgroundColor='#0A1A2A'
		BackgroundOpacity='1.00'
		Location='0,24'
		Name='content'
		ScrollExtent='320,260'
		Size='320,260'
	>
		<!-- Color Wheel Image with circular frame -->
		<Page
			AbsorbsInput='true'
			BackgroundColor='#0A1A2A'
			BackgroundOpacity='1.00'
			GetsInput='true'
			Location='60,10'
			Name='pageWheel'
			RStyleDefault='/Styles.New.whiteOutline.rs_default'
			ScrollExtent='200,200'
			Size='200,200'
		>
			<Image
				Location='0,0'
				Name='imgWheel'
				ScrollExtent='200,200'
				Size='200,200'
				SourceResource='ui_color_wheel'
				Stretch='true'
			/>
			<Page
				BackgroundColor='#FFFFFF'
				BackgroundOpacity='1.00'
				Location='100,100'
				Name='cursorWheel'
				ScrollExtent='8,8'
				Size='8,8'
			/>
		</Page>

		<!-- RGB Input Row -->
		<Page
			Location='10,215'
			Name='rgbRow'
			ScrollExtent='300,24'
			Size='300,24'
		>
			<Text Font='bold_11' LocalText='R:' Location='0,4' ScrollExtent='18,16' Size='18,16' TextColor='#FF6666'>R:</Text>
			<Textbox
				BackgroundColor='#330000'
				Editable='true'
				Font='bold_12'
				GetsInput='true'
				LocalText='255'
				Location='20,0'
				MaxLength='3'
				Name='textboxR'
				NumericInteger='true'
				ScrollExtent='45,22'
				Size='45,22'
				Style='/Styles.textbox.default.style'
				TextColor='#FFFFFF'
			>255</Textbox>

			<Text Font='bold_11' LocalText='G:' Location='80,4' ScrollExtent='18,16' Size='18,16' TextColor='#66FF66'>G:</Text>
			<Textbox
				BackgroundColor='#003300'
				Editable='true'
				Font='bold_12'
				GetsInput='true'
				LocalText='255'
				Location='100,0'
				MaxLength='3'
				Name='textboxG'
				NumericInteger='true'
				ScrollExtent='45,22'
				Size='45,22'
				Style='/Styles.textbox.default.style'
				TextColor='#FFFFFF'
			>255</Textbox>

			<Text Font='bold_11' LocalText='B:' Location='160,4' ScrollExtent='18,16' Size='18,16' TextColor='#6666FF'>B:</Text>
			<Textbox
				BackgroundColor='#000033'
				Editable='true'
				Font='bold_12'
				GetsInput='true'
				LocalText='255'
				Location='180,0'
				MaxLength='3'
				Name='textboxB'
				NumericInteger='true'
				ScrollExtent='45,22'
				Size='45,22'
				Style='/Styles.textbox.default.style'
				TextColor='#FFFFFF'
			>255</Textbox>
		</Page>
	</Page>

	<!-- HTML Color Input Row -->
	<Page
		Location='10,288'
		Name='htmlRow'
		ScrollExtent='300,32'
		Size='300,32'
	>
		<Text Font='bold_11' LocalText='HTML:' Location='0,8' ScrollExtent='40,16' Size='40,16' TextColor='#AADDFF'>HTML:</Text>
		<Textbox
			BackgroundColor='#1A2A3A'
			Editable='true'
			Font='bold_14'
			GetsInput='true'
			LocalText='#FFFFFF'
			Location='45,3'
			MaxLength='7'
			Name='textboxHtml'
			ScrollExtent='80,26'
			Size='80,26'
			Style='/Styles.textbox.default.style'
			TextColor='#FFFFFF'
		>#FFFFFF</Textbox>
		<Text Font='bold_11' LocalText='Preview:' Location='140,8' ScrollExtent='55,16' Size='55,16' TextColor='#AAAAAA'>Preview:</Text>
		<Page
			BackgroundColor='#FFFFFF'
			BackgroundOpacity='1.00'
			BackgroundTint='#FFFFFF'
			Location='200,3'
			Name='pageSample'
			RStyleDefault='/Styles.New.whiteOutline.rs_default'
			ScrollExtent='90,26'
			Size='90,26'
		/>
	</Page>
</Page>
