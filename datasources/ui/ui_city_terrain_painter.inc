<Page
	BackgroundColor='#000000'
	BackgroundOpacity='0.70'
	Name='CityTerrainPainter'
	PackSize='1,1'
	ScrollExtent='1024,768'
	Size='1024,768'
	Visible='false'
	VisualEditLock='true'
>
	<Page
		BackgroundColor='#404040'
		BackgroundOpacity='0.95'
		BackgroundTint='#00354F'
		Location='200,60'
		MinimumSize='440,672'
		Name='terrainPainterWindow'
		PalBgTint='back1'
		ScrollExtent='440,672'
		Size='440,672'
		UserMovable='true'
		UserResizable='false'
	>
		<Data
			Name='CodeData'
			comboShader='content.comboShader'
			comboPaintMode='content.comboPaintMode'
			sliderRadius='content.pageCircleOptions.sliderRadius'
			sliderBlend='content.sliderBlend'
			textRadius='content.pageCircleOptions.textRadius'
			textBlend='content.textBlend'
			textInstructions='content.textInstructions'
			textStatus='content.textStatus'
			listRegions='content.listRegions'
			imagePreview='content.imagePreview'
			buttonApply='content.buttonApply'
			buttonCancel='content.buttonCancel'
			buttonUndo='content.buttonUndo'
			pageCircleOptions='content.pageCircleOptions'
			pageLineOptions='content.pageLineOptions'
			pageFlattenOptions='content.pageFlattenOptions'
			sliderWidth='content.pageLineOptions.sliderWidth'
			textWidth='content.pageLineOptions.textWidth'
			buttonSetMarker='content.pageLineOptions.buttonSetMarker'
			sliderHeight='content.pageFlattenOptions.sliderHeight'
			textHeight='content.pageFlattenOptions.textHeight'
			sliderFlattenRadius='content.pageFlattenOptions.sliderFlattenRadius'
			textFlattenRadius='content.pageFlattenOptions.textFlattenRadius'
			buttonUseCurrentHeight='content.pageFlattenOptions.buttonUseCurrentHeight'
			buttonUseCityRadius='content.pageFlattenOptions.buttonUseCityRadius'
			checkShowTileGrid='content.checkShowTileGrid'
			buttonClose='header.buttonClose'
		/>

		<Page BackgroundColor='#FFFFFF' BackgroundOpacity='0.50' BackgroundTint='#00354F' Location='0,0' Name='header' PalBgTint='back1' RStyleDefault='/Styles.New.whiteOutline.rs_default' ScrollExtent='440,32' Size='440,32'>
			<Text Enabled='false' Font='bold_14' LocalText='City Terrain Painter' Location='10,6' Name='titleText' ScrollExtent='380,20' Size='380,20' TextAlignment='Left' TextColor='#FFFFFF'>City Terrain Painter</Text>
			<Button LocalText='X' Location='410,4' Name='buttonClose' ScrollExtent='24,24' Size='24,24' Style='/Styles.New.buttons.hud.style'>X</Button>
		</Page>

		<Page GetsInput='true' Location='10,42' Name='content' ScrollExtent='420,616' Size='420,616'>
			<Text Enabled='false' Font='bold_11' LocalText='Select paint mode and options, then position yourself and click Apply.' Location='0,0' Name='textInstructions' ScrollExtent='420,30' Size='420,30' TextAlignment='Center' TextColor='#FFFF00'>Select paint mode and options, then position yourself and click Apply.</Text>

			<Text Enabled='false' Font='bold_11' LocalText='Paint Mode:' Location='0,36' Name='labelPaintMode' ScrollExtent='100,20' Size='100,20' TextAlignment='Left' TextColor='#FFFFFF'>Paint Mode:</Text>
			<ComboBox
				BackgroundOpacity='0.70'
				ButtonMargin='2,2,2,0'
				ButtonStyle='/Styles.New.buttons.combo_arrow.style'
				ButtonWidth='10'
				DataSource='dsPaintMode'
				DropDownHeight='100'
				Editable='false'
				GetsInput='true'
				ListStyle='/Styles.list.combo.style_all'
				Location='110,34'
				MinimumSize='0,22'
				Name='comboPaintMode'
				RStyleDefault='/Styles.New.combo.combo1.rs_default'
				ScrollbarStyle='/Styles.scrollbar.default.vHud3.style'
				ScrollbarWidth='11'
				ScrollExtent='180,22'
				SelectedIndex='0'
				Size='180,22'
				TextboxMargin='0,1,0,0'
				TextboxStyle='/Styles.textbox.combo.Style_new'
			/>
			<DataSource Name='dsPaintMode'>
				<Data Name='0' Text='Circle'/>
				<Data Name='1' Text='Line/Road'/>
				<Data Name='2' Text='Flatten'/>
			</DataSource>

			<Text Enabled='false' Font='bold_11' LocalText='Shader:' Location='0,66' Name='labelShader' ScrollExtent='100,20' Size='100,20' TextAlignment='Left' TextColor='#FFFFFF'>Shader:</Text>
			<ComboBox
				BackgroundOpacity='0.70'
				ButtonMargin='2,2,2,0'
				ButtonStyle='/Styles.New.buttons.combo_arrow.style'
				ButtonWidth='10'
				DataSource='dsShader'
				DropDownHeight='300'
				Editable='false'
				GetsInput='true'
				ListStyle='/Styles.list.combo.style_all'
				Location='110,64'
				MinimumSize='0,22'
				Name='comboShader'
				RStyleDefault='/Styles.New.combo.combo1.rs_default'
				ScrollbarStyle='/Styles.scrollbar.default.vHud3.style'
				ScrollbarWidth='11'
				ScrollExtent='300,22'
				SelectedIndex='0'
				Size='300,22'
				TextboxMargin='0,1,0,0'
				TextboxStyle='/Styles.textbox.combo.Style_new'
			/>
			<DataSource Name='dsShader'>
				<Data Name='0' Text='Loading...'/>
			</DataSource>

			<Text Enabled='false' Font='bold_11' LocalText='Blend Dist:' Location='0,96' Name='labelBlend' ScrollExtent='100,20' Size='100,20' TextAlignment='Left' TextColor='#FFFFFF'>Blend Dist:</Text>
			<Sliderbar
				Location='110,96'
				Name='sliderBlend'
				ScrollExtent='220,18'
				Size='220,18'
				Style='/Styles.New.Slider.default.horizontal.style_test'
				LowerLimit='0'
				UpperLimit='20'
				Value='5'
			/>
			<Textbox
				BackgroundOpacity='0.70'
				BackgroundTint='#1CFFFF'
				Location='340,94'
				Name='textBlend'
				NumericInteger='true'
				PalBgTint='line1'
				RStyleDefault='/Styles.New.whiteOutline_palW.rs_default'
				ScrollExtent='50,22'
				Size='50,22'
				Style='/Styles.textbox.default.Style'
				Text='5'
				TextAlignment='Center'
			>5</Textbox>

			<Checkbox
				Color='#FFFFFF'
				Location='0,122'
				Name='checkShowTileGrid'
				ScrollExtent='400,18'
				Size='400,18'
				Style='/Styles.New.Checkbox2.Style'
				LocalText='Show terrain tile grid (around camera)'
			>Show terrain tile grid (around camera)</Checkbox>

			<Page Location='0,148' Name='pageCircleOptions' ScrollExtent='420,36' Size='420,36' Visible='true'>
				<Text Enabled='false' Font='bold_11' LocalText='Radius:' Location='0,0' Name='labelRadius' ScrollExtent='100,20' Size='100,20' TextAlignment='Left' TextColor='#FFFFFF'>Radius:</Text>
				<Sliderbar
					Location='110,0'
					Name='sliderRadius'
					ScrollExtent='220,18'
					Size='220,18'
					Style='/Styles.New.Slider.default.horizontal.style_test'
					LowerLimit='5'
					UpperLimit='50'
					Value='10'
				/>
				<Textbox
					BackgroundOpacity='0.70'
					BackgroundTint='#1CFFFF'
					Location='340,0'
					Name='textRadius'
					NumericInteger='true'
					PalBgTint='line1'
					RStyleDefault='/Styles.New.whiteOutline_palW.rs_default'
					ScrollExtent='50,22'
					Size='50,22'
					Style='/Styles.textbox.default.Style'
					Text='10'
					TextAlignment='Center'
				>10</Textbox>
			</Page>

			<Page Location='0,190' Name='pageLineOptions' ScrollExtent='420,70' Size='420,70' Visible='false'>
				<Text Enabled='false' Font='bold_11' LocalText='Road Width:' Location='0,0' Name='labelWidth' ScrollExtent='100,20' Size='100,20' TextAlignment='Left' TextColor='#FFFFFF'>Road Width:</Text>
				<Sliderbar
					Location='110,0'
					Name='sliderWidth'
					ScrollExtent='220,18'
					Size='220,18'
					Style='/Styles.New.Slider.default.horizontal.style_test'
					LowerLimit='2'
					UpperLimit='20'
					Value='4'
				/>
				<Textbox
					BackgroundOpacity='0.70'
					BackgroundTint='#1CFFFF'
					Location='340,0'
					Name='textWidth'
					NumericInteger='true'
					PalBgTint='line1'
					RStyleDefault='/Styles.New.whiteOutline_palW.rs_default'
					ScrollExtent='50,22'
					Size='50,22'
					Style='/Styles.textbox.default.Style'
					Text='4'
					TextAlignment='Center'
				>4</Textbox>
				<Button LocalText='Set Marker' Location='110,32' Name='buttonSetMarker' ScrollExtent='150,26' Size='150,26' Style='/Styles.New.buttons.hud.style'>Set Marker</Button>
			</Page>

			<Page Location='0,190' Name='pageFlattenOptions' ScrollExtent='420,100' Size='420,100' Visible='false'>
				<Text Enabled='false' Font='bold_11' LocalText='Target Height:' Location='0,0' Name='labelHeight' ScrollExtent='100,20' Size='100,20' TextAlignment='Left' TextColor='#FFFFFF'>Target Height:</Text>
				<Sliderbar
					Location='110,0'
					Name='sliderHeight'
					ScrollExtent='180,18'
					Size='180,18'
					Style='/Styles.New.Slider.default.horizontal.style_test'
					LowerLimit='-100'
					UpperLimit='500'
					Value='0'
				/>
				<Textbox
					BackgroundOpacity='0.70'
					BackgroundTint='#1CFFFF'
					Location='300,0'
					Name='textHeight'
					NumericInteger='true'
					NumericNegative='true'
					PalBgTint='line1'
					RStyleDefault='/Styles.New.whiteOutline_palW.rs_default'
					ScrollExtent='50,22'
					Size='50,22'
					Style='/Styles.textbox.default.Style'
					Text='0'
					TextAlignment='Center'
				>0</Textbox>
				<Button LocalText='Use Current' Location='355,0' Name='buttonUseCurrentHeight' ScrollExtent='60,22' Size='60,22' Style='/Styles.New.buttons.hud.style'>Use Current</Button>
				<Text Enabled='false' Font='bold_11' LocalText='Flatten Radius:' Location='0,32' Name='labelFlattenRadius' ScrollExtent='100,20' Size='100,20' TextAlignment='Left' TextColor='#FFFFFF'>Flatten Radius:</Text>
				<Sliderbar
					Location='110,32'
					Name='sliderFlattenRadius'
					ScrollExtent='180,18'
					Size='180,18'
					Style='/Styles.New.Slider.default.horizontal.style_test'
					LowerLimit='10'
					UpperLimit='500'
					Value='100'
				/>
				<Textbox
					BackgroundOpacity='0.70'
					BackgroundTint='#1CFFFF'
					Location='300,30'
					Name='textFlattenRadius'
					NumericInteger='true'
					PalBgTint='line1'
					RStyleDefault='/Styles.New.whiteOutline_palW.rs_default'
					ScrollExtent='50,22'
					Size='50,22'
					Style='/Styles.textbox.default.Style'
					Text='100'
					TextAlignment='Center'
				>100</Textbox>
				<Button LocalText='Use City Radius' Location='355,30' Name='buttonUseCityRadius' ScrollExtent='60,22' Size='60,22' Style='/Styles.New.buttons.hud.style'>City Radius</Button>
				<Text Enabled='false' Font='bold_9' LocalText='Note: Flatten will smooth terrain to the target height within the radius.' Location='0,62' Name='textFlattenNote' ScrollExtent='420,24' Size='420,24' TextAlignment='Left' TextColor='#AAAAAA'>Note: Flatten will smooth terrain to the target height within the radius.</Text>
			</Page>

			<Text Enabled='false' Font='bold_11' LocalText='Preview:' Location='0,300' Name='labelPreview' ScrollExtent='100,20' Size='100,20' TextAlignment='Left' TextColor='#FFFFFF'>Preview:</Text>
			<Image BackgroundColor='#333333' BackgroundOpacity='0.50' Location='110,300' Name='imagePreview' ScrollExtent='80,80' Size='80,80' SourceResource=''/>

			<Text Enabled='false' Font='bold_11' LocalText='Status' Location='0,390' Name='labelStatus' ScrollExtent='420,16' Size='420,16' TextAlignment='Left' TextColor='#CCCCCC'>Status</Text>
			<Text Enabled='false' Font='bold_11' LocalText='' Location='0,408' Name='textStatus' ScrollExtent='420,22' Size='420,22' TextAlignment='Left' TextAlignmentVertical='Top' TextColor='#00FF00'></Text>

			<Text Enabled='false' Font='bold_11' LocalText='All terrain edits for this city (select one to remove)' Location='0,434' Name='labelRegionList' ScrollExtent='420,16' Size='420,16' TextAlignment='Left' TextColor='#CCCCCC'>All terrain edits for this city (select one to remove)</Text>
			<List
				BackgroundColor='#000000'
				BackgroundOpacity='0.60'
				CellHeight='26'
				GetsInput='true'
				Horizontal='false'
				Location='0,452'
				Name='listRegions'
				RStyleDefault='/Styles.New.whiteOutline.rs_default'
				ScrollExtent='420,120'
				Size='420,120'
				Style='/Styles.list.default.listbox.style'
				SelectionAllowedMultiRow='false'
			/>

			<Button Enabled='false' LocalText='Remove' Location='8,580' Name='buttonUndo' ScrollExtent='100,28' Size='100,28' Style='/Styles.New.buttons.hud.style'>Remove</Button>
			<Button LocalText='Apply' Location='120,580' Name='buttonApply' ScrollExtent='100,28' Size='100,28' Style='/Styles.New.buttons.hud.style'>Apply</Button>
			<Button LocalText='Cancel' Location='240,580' Name='buttonCancel' ScrollExtent='100,28' Size='100,28' Style='/Styles.New.buttons.hud.style'>Cancel</Button>
		</Page>
	</Page>
</Page>
